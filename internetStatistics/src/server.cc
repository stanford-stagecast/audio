#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>
#include <unistd.h>

#include "eventloop.hh"
#include "exception.hh"
#include "socket.hh"
#include "timer.hh"

const uint64_t DELAY{ 1'000'000 };

using namespace std;

void split_on_char(const string_view str, const char ch_to_find, vector<string_view>& ret)
{
  ret.clear();
  bool in_double_quoted_string = false;
  unsigned int field_start = 0;
  for (unsigned int i = 0; i < str.size(); i++) {
    const char ch = str[i];
    if (ch == '"') {
      in_double_quoted_string = !in_double_quoted_string;
    }
    else if (in_double_quoted_string) {
      continue;
    }
    else if (ch == ch_to_find) {
      ret.emplace_back(str.substr(field_start, i - field_start));
      field_start = i + 1;
    }
  }
  ret.emplace_back(str.substr(field_start));
}

void export_data(vector<int64_t>& buffer_vals, vector<int>& packets_received)
{
  std::fstream fout;
  fout.open("buffer_every_one_ms.csv", std::ios::out);
  for (size_t i = 0; i < buffer_vals.size(); i++) {
    fout << buffer_vals.at(i) << "\n";
  }
  fout.close();

  fout.open("packets_received.csv", std::ios::out);
  for (size_t i = 0; i < packets_received.size(); i++) {
    fout << packets_received.at(i) << "\n";
  }
  fout.close();
}

void program_body(vector<int64_t>& buffer_vals, vector<int>& packets_received)
{
  EventLoop event_loop;
  UDPSocket server_sock;
  server_sock.set_blocking(true);
  server_sock.bind({ "0", 9090 });

  uint64_t start_time = Timer::timestamp_ns();
  uint64_t prev_time = start_time;
  uint64_t current_time = start_time;
  atomic<uint64_t> server_packet_counter = 1;
  atomic<int64_t> buffer = 0;
  std::mutex mtx_buf_vec;
  std::mutex mtx_packet_vec;
  bool received = false;

  event_loop.add_rule(
    "Server receive packets and keep track of buffer",
    server_sock,
    Direction::In,
    [&] {
      received = true;
      if (prev_time + DELAY < current_time) {
        buffer--;
        auto recv = server_sock.recv();
        string payload = recv.payload;
        uint64_t packet_number = stoull(payload);

        if (packet_number == server_packet_counter) {
          buffer++;
          server_packet_counter++;

          unique_lock<mutex> lk(mtx_packet_vec);
          packets_received.push_back(1);
        }
        else if (packet_number > server_packet_counter) {
          cout << "skipping packets " << server_packet_counter << " to " << packet_number - 1 << endl;
          buffer += (server_packet_counter - packet_number) + 1;
          unique_lock<mutex> lk(mtx_packet_vec);
          for (size_t i = server_packet_counter; i < packet_number; ++i) {
            packets_received.push_back(0);
          }
          packets_received.push_back(1);
          server_packet_counter = packet_number + 1;
        }
        prev_time = current_time;
        unique_lock<mutex> lk(mtx_buf_vec);
        buffer_vals.push_back(buffer);
        cout << "received packet " << packet_number << " current buffer " << buffer << endl;
      }
      current_time = Timer::timestamp_ns();
    },
    [&] { return server_packet_counter < 100000; });

  while (event_loop.wait_next_event(5) != EventLoop::Result::Exit) {
    if (received && prev_time + DELAY < current_time) { // TODO: Determine whether this section is necessary
      // buffer--; // race condition? not sure how the event handler is implemented
      // cout << "buffer: " << buffer << endl;
      // prev_time = current_time;
    }
    current_time = Timer::timestamp_ns();
  }
}

int main()
{
  try {
    global_timer();
    vector<int64_t> buffer_values;
    vector<int> packets_received;
    program_body(buffer_values, packets_received);
    export_data(buffer_values, packets_received);
    cout << global_timer().summary() << "\n";
  }
  catch (const exception& e) {
    cout << "Exception: " << e.what() << endl;
    cout << global_timer().summary() << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
