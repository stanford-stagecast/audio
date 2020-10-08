#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <atomic>
#include <mutex>
#include <unistd.h>

#include "eventloop.hh"
#include "exception.hh"
#include "socket.hh"
#include "timer.hh"

const uint64_t DELAY {1'000'000};

using namespace std;

void split_on_char( const string_view str, const char ch_to_find, vector<string_view>& ret )
{
  ret.clear();
  bool in_double_quoted_string = false;
  unsigned int field_start = 0;
  for ( unsigned int i = 0; i < str.size(); i++ ) {
    const char ch = str[i];
    if ( ch == '"' ) {
      in_double_quoted_string = !in_double_quoted_string;
    } else if ( in_double_quoted_string ) {
      continue;
    } else if ( ch == ch_to_find ) {
      ret.emplace_back( str.substr( field_start, i - field_start ) );
      field_start = i + 1;
    }
  }
  ret.emplace_back( str.substr( field_start ) );
}

void export_data(vector<int64_t>& buffer_vals) {
  std::fstream fout;
  fout.open("buffer_every_one_ms.csv", std::ios::out|std::ios::app);
  for (size_t i = 0; i < buffer_vals.size(); i++) {
    fout << buffer_vals.at(i) << "\n";
  }

}

void program_body(vector<int64_t>& buffer_vals)
{
  EventLoop event_loop;
  UDPSocket server_sock;
  server_sock.set_blocking(true);
  server_sock.bind({"0", 9090});

  uint64_t start_time = Timer::timestamp_ns();
  uint64_t prev_time = start_time;
  uint64_t current_time = start_time;
  uint64_t server_packet_counter = 1;
  atomic<int64_t> buffer = 0;
  std::mutex mtx_buf_vec;
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
          } else if (packet_number > server_packet_counter) {
            cout << "skipping packets " << server_packet_counter << " to " << packet_number - 1 << endl;
            buffer += (server_packet_counter - packet_number) + 1;
            server_packet_counter = packet_number + 1;
          }
          prev_time = current_time;
	  mtx_buf_vec.lock();
	  buffer_vals.push_back(buffer);
	  mtx_buf_vec.unlock();
          cout << "received packet " << packet_number << " current buffer " << buffer << endl;
	}
        current_time = Timer::timestamp_ns();
    },
    [&] {return server_packet_counter < 100000;});

  while (event_loop.wait_next_event(5) != EventLoop::Result::Exit) {
    if (received && prev_time + DELAY < current_time) { //TODO: Determine whether this section is necessary
      //buffer--; // race condition? not sure how the event handler is implemented
      //cout << "buffer: " << buffer << endl;
      //prev_time = current_time;
    }
    current_time = Timer::timestamp_ns();
  }
}


int main() {
  try {
    global_timer();
    vector<int64_t> buffer_values;
    program_body(buffer_values);
    export_data(buffer_values);
    cout << global_timer().summary() << "\n";
  } catch (const exception& e) {
    cout << "Exception: " << e.what() << endl;
    cout << global_timer().summary() << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
