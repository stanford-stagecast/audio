#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <unistd.h>

#include "eventloop.hh"
#include "exception.hh"
#include "server_socket.hh"
#include "timer.hh"

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
  UDPSocket server_sock;
  server_sock.bind({"0", 9090});

  uint64_t server_packet_counter = 1;
  event_loop.add_rule(
    "Server receive packets and keep track of buffer",
    server_sock,
    Direction::In,
    [&] {
      auto recv = server_sock.recv();
      cout << "Datagram received from " << rec.source_address.to_string() << ": " << rec.payload << "\n";
    /*  vector<string_view> fields;
      split_on_char(rec.payload, ' ', fields);
      if (fields.size() > 0 && server_packet_counter < fields[0]) {
        server_packet_counter = fields[0];
      }*/
    },
    [&] {return server_packet_counter;});
  while (event_loop.wait_next_event(1) != EventLoop::Result::Exit) {
    if (Timer::timestamp_ns() - start_time > 5ULL * 1000 * 1000 * 1000) {
      cout << "Timeout\n";
    }
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
