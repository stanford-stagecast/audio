#include "eventloop.hh"
#include "exception.hh"
#include "socket.hh"
#include "timer.hh"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <thread>
#include <unistd.h>

using namespace std;
using namespace std::chrono;

const uint64_t DEFAULT_NUM_PACKETS = 10000;

const string SOPHON_ADDR = "171.67.76.94";
const string LOCALHOST_ADDR = "127.0.0.1";

Address server { SOPHON_ADDR, 9090 };

/**
 * Converts the given packet number to a string and pads to 40 bytes with spaces.
 */
string build_packet( int packet_number )
{
  static const int packet_size = 40;

  string s = std::to_string( packet_number );
  return s + string( packet_size - s.length(), ' ' );
}

/**
 * Sends a UDP packet with a 40 byte payload to the server at sophon.stanford.edu every 2.5 milliseconds, stopping
 * after sending `num_packets` times. Each payload consists of a numeric packet counter followed by whitespace
 * padding to 40 bytes.
 */
void program_body( unsigned int num_packets )
{
  cout << "Preparing to send " << num_packets << " packets" << endl;

  UDPSocket client_sock;
  client_sock.set_blocking( false );

  auto start_time = steady_clock::now();
  const auto transmission_interval = microseconds( 2500 );

  auto time_for_next_transmission = start_time + transmission_interval;
  start_time = time_for_next_transmission;

  for ( unsigned int packet_counter = 0; packet_counter < num_packets; packet_counter++ ) {
    auto duration_to_sleep = time_for_next_transmission - steady_clock::now();
    if ( duration_to_sleep.count() > 0 ) {
      this_thread::sleep_for( duration_to_sleep );
    }
    time_for_next_transmission += transmission_interval;

    // Output progress
    if ( packet_counter % 1000 == 0 )
      cout << "Packet counter: " << packet_counter << endl;
    string packet_content = build_packet( packet_counter );
    client_sock.sendto( server, packet_content );
  }

  auto end_time = steady_clock::now();
  chrono::duration<double, ratio<1, 1000>> overall_duration = end_time - start_time;
  cout << "Time between packet 0 and packet " << num_packets << ": " << overall_duration.count() << " ms" << endl;
}

int main( int argc, char* argv[] )
{
  try {
    global_timer();

    // Parse arguments for number of packets to send
    unsigned int num_packets;
    if ( argc < 2 ) {
      num_packets = DEFAULT_NUM_PACKETS;
    } else {
      char* endptr;
      num_packets = strtoul( argv[1], &endptr, 10 );
      if ( *endptr != '\0' ) {
        throw runtime_error( "Invalid number of packets provided" );
      }
    }

    program_body( num_packets );
    cout << "\n" << global_timer().summary() << "\n";
  } catch ( const exception& e ) {
    cout << "Exception: " << e.what() << endl;
    cout << global_timer().summary() << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
