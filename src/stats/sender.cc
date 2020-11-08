#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <unistd.h>

#include "alsa_devices.hh"
#include "audio_device_claim.hh"
#include "eventloop.hh"
#include "exception.hh"
#include "socket.hh"
#include "timer.hh"
#include "typed_ring_buffer.hh"

using namespace std;
using namespace std::chrono;

const uint64_t DEFAULT_NUM_PACKETS = 10000;
const uint32_t SAMPLES_INTERVAL = 120;

const string SOPHON_ADDR = "171.67.76.94";
const string LOCALHOST_ADDR = "127.0.0.1";

const Address server { SOPHON_ADDR, 9090 };

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
  ios::sync_with_stdio( false );

  cout << "Preparing to send " << num_packets << " packets" << endl;

  UDPSocket client_sock;
  client_sock.set_blocking( false );

  const auto [name, interface_name] = ALSADevices::find_device( "UAC-2, USB Audio" );
  const auto device_claim = AudioDeviceClaim::try_claim( name );
  AudioPair uac2 { interface_name };
  uac2.initialize();

  EventLoop loop;

  // FileDescriptor input { CheckSystemCall( "dup STDIN_FILENO", dup( STDIN_FILENO ) ) };

  uint32_t packet_counter = 0;
  uint32_t num_samples = 0;
  auto start_time = steady_clock::now();

  AudioBuffer audio_output { 65536 };

  auto loopback_rule = loop.add_rule(
    "send packets",
    uac2.fd(),
    Direction::In,
    [&] {
      num_samples += uac2.mic_avail();
      uac2.loopback( audio_output );
      if ( num_samples >= SAMPLES_INTERVAL ) {
        auto cur_time = steady_clock::now();
        chrono::duration<double, ratio<1, 1000>> diff = cur_time - start_time;
        start_time = cur_time;
        string packet_content = build_packet( packet_counter );
        client_sock.sendto( server, packet_content );
        num_samples = 0;
        packet_counter++;
      }
    },
    [&] { return packet_counter < num_packets; },
    [] {},
    [&] {
      uac2.recover();
      return true;
    } );

  auto buffer_rule = loop.add_rule(
    "read from buffer",
    [&] {
      audio_output.ch1.pop( audio_output.ch1.num_stored() );
      audio_output.ch2.pop( audio_output.ch2.num_stored() );
    },
    [&] { return audio_output.ch1.num_stored() > 0 && packet_counter < num_packets; } );

  uac2.start();
  while ( loop.wait_next_event( -1 ) != EventLoop::Result::Exit ) {
  }

  cout << loop.summary() << "\n";
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
