#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <atomic>

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

void program_body()
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
  vector<int64_t> buffer_vals;
  bool received = false;

  event_loop.add_rule(
    "Server receive packets and keep track of buffer",
    server_sock,
    Direction::In,
    [&] {
        received = true;
      // auto recv = server_sock.recv();
      // cout << "received packet from " << recv.source_address << ": " << stoull(recv.payload) << endl;
      // if (prev_time + DELAY < current_time) {
        auto recv = server_sock.recv();
        string payload = recv.payload;
        uint64_t packet_number = stoull(payload);
        cout << "received packet " << packet_number << endl;

        if (packet_number == server_packet_counter) {
          buffer++;
          server_packet_counter++;
        }
        else if (packet_number > server_packet_counter) {
          cout << "skipping packets " << server_packet_counter << " to " << packet_number - 1 << endl;
          buffer += (server_packet_counter - packet_number) + 1;
          server_packet_counter = packet_number + 1;
        }

        // prev_time = current_time;
      // }
      // current_time = Timer::timestamp_ns();
    },
    [&] {return server_packet_counter;});

  while (event_loop.wait_next_event(5) != EventLoop::Result::Exit) {
    if (received && prev_time + DELAY < current_time) {
      buffer--; // race condition? not sure how the event handler is implemented
      cout << "buffer: " << buffer << endl;
      prev_time = current_time;
    }
    current_time = Timer::timestamp_ns();
  }
}


int main() {
  try {
    global_timer();
    program_body();
    cout << global_timer().summary() << "\n";
  } catch (const exception& e) {
    cout << "Exception: " << e.what() << endl;
    cout << global_timer().summary() << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
