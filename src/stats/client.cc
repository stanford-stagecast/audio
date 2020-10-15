#include "eventloop.hh"
#include "exception.hh"
#include "socket.hh"
#include "timer.hh"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <thread>
#include <unistd.h>

using namespace std;
using namespace std::chrono;

const uint64_t MAX_NUM_PACKETS = 10000;
const uint64_t DELAY { 1'000'000 };

const string SOPHON_ADDR = "171.67.76.94";
const string LOCALHOST_ADDR = "127.0.0.1";

Address server { SOPHON_ADDR, 9090 };

string build_packet( int packet_counter )
{
  return std::to_string( packet_counter ); // 32 byte packet
}

void program_body()
{
  UDPSocket client_sock;
  client_sock.set_blocking( false );

  const auto transmission_interval = microseconds( 2500 );

  auto time_for_next_transmission = steady_clock::now() + transmission_interval;

  for ( unsigned int packet_counter = 0; packet_counter < MAX_NUM_PACKETS; packet_counter++ ) {
    auto duration_to_sleep = time_for_next_transmission - steady_clock::now();
    if ( duration_to_sleep.count() > 0 ) {
      this_thread::sleep_for( duration_to_sleep );
    }
    time_for_next_transmission += transmission_interval;

    if ( packet_counter % 1000 == 0 )
      cout << "Packet counter: " << packet_counter << endl;
    string packet_content = build_packet( packet_counter );
    client_sock.sendto( server, packet_content );
  }
}

int main()
{
  try {
    global_timer();
    program_body();
    cout << global_timer().summary() << "\n";
  } catch ( const exception& e ) {
    cout << "Exception: " << e.what() << endl;
    cout << global_timer().summary() << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
