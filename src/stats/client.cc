#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <pthread.h>
#include <thread>
#include <unistd.h>

#include "alsa_devices.hh"
#include "audio_device_claim.hh"
#include "eventloop.hh"
#include "exception.hh"
#include "socket.hh"
#include "timer.hh"
#include "typed_ring_buffer.hh"

using namespace std;
using namespace chrono;

const uint32_t SAMPLE_RATE_MS = 48;
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

void program_body()
{
  ios::sync_with_stdio( false );

  const auto [name, interface_name] = ALSADevices::find_device( "UAC-2, USB Audio" );
  const auto device_claim = AudioDeviceClaim::try_claim( name );
  AudioPair uac2 { interface_name };
  uac2.initialize();

  UDPSocket client_sock;
  client_sock.set_blocking( false );
  client_sock.bind( { "0", 9091 } );

  client_sock.sendto( server, "hello" );

  AudioBuffer audio_output { 65536 };
  EventLoop loop;
  bool first_packet_received = false;
  uint32_t num_samples;
  uint64_t send_packet_counter = 0;
  uint64_t receive_packet_counter = 0;
  uint64_t buffer = 0;

  auto loopback_rule = loop.add_rule(
    "loopback and send",
    uac2.fd(),
    Direction::In,
    [&] {
      uac2.loopback( audio_output );
      num_samples += uac2.mic_avail();

      if ( num_samples >= SAMPLES_INTERVAL ) {
        // TODO: playback 2.5 ms from buffer
        string packet_content = build_packet( send_packet_counter );
        client_sock.sendto( server, packet_content );

        if ( send_packet_counter % 1000 == 0 )
          cout << "Sent client packet #" << send_packet_counter << endl;
        num_samples = 0;
        send_packet_counter++;
      }
    },
    [&] { return true; },
    [] {},
    [&] {
      uac2.recover();
      return true;
    } );

  auto receive_rule = loop.add_rule(
    "receiver",
    client_sock,
    Direction::In,
    [&] {
      auto recv = client_sock.recv();
      string payload = recv.payload;
      uint64_t packet_number = stoull( payload );
      if ( !first_packet_received ) {
        cout << "First packet received: " << packet_number << endl;
        receive_packet_counter = packet_number + 1;
        first_packet_received = true;
      } else if ( packet_number >= receive_packet_counter ) {
        if ( packet_number % 1000 == 0 )
          cout << "Received server packet #" << packet_number << endl;
        buffer += ( packet_number - receive_packet_counter + 1 ) * 2.5;
        // for ( size_t i = receive_packet_counter; i < packet_number; i++ ) {
        //   packets_received.push_back( 0 );
        //   silent_packets++;
        // }
        // packets_received.push_back( 1 );
        receive_packet_counter = packet_number + 1;
      }
    },
    [&] { return true; } );

  auto buffer_rule = loop.add_rule(
    "read from buffer",
    [&] {
      audio_output.ch1.pop( audio_output.ch1.num_stored() );
      audio_output.ch2.pop( audio_output.ch2.num_stored() );
    },
    [&] { return audio_output.ch1.num_stored() > 0; } );

  // End on keyboard input
  FileDescriptor input { CheckSystemCall( "dup STDIN_FILENO", dup( STDIN_FILENO ) ) };
  loop.add_rule( "exit on keystroke", input, Direction::In, [&] {
    loopback_rule.cancel();
    receive_rule.cancel();
    input.close();
  } );

  uac2.start();
  while ( loop.wait_next_event( -1 ) != EventLoop::Result::Exit ) {
  }

  cout << loop.summary() << "\n";
}

int main()
{
  try {
    program_body();
    cout << global_timer().summary() << "\n";
  } catch ( const exception& e ) {
    cout << "Exception: " << e.what() << "\n";
    cout << global_timer().summary() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
