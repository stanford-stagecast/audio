#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>
#include <pthread.h>
#include <thread>
#include <unistd.h>

#include "alsa_devices.hh"
#include "eventloop.hh"
#include "exception.hh"
#include "socket.hh"
#include "timer.hh"
#include "typed_ring_buffer.hh"

#ifndef NDBUS
#include "device_claim_util.hh"
#endif

using namespace std;
using namespace chrono;

const string HOME( getenv( "HOME" ) );

const string BUFFER_CSV = HOME + "/audio/csv/buffer_zoom_test.csv";
const string PACKET_CSV = HOME + "/audio/csv/packets_zoom_test.csv";

const uint64_t DEFAULT_NUM_PACKETS = 10000;
const uint32_t SAMPLE_RATE_MS = 48;
const uint32_t SAMPLES_INTERVAL = 120;
const string SOPHON_ADDR = "171.67.76.94";
const string LOCALHOST_ADDR = "127.0.0.1";

void program_body( size_t num_packets, vector<double>& buffer_vals, vector<int>& packets_received )
{
  cout << "Preparing to receive " << num_packets << " packets" << endl;
  ios::sync_with_stdio( false );

  const auto [name, interface_name] = find_device( "UAC-2, USB Audio" );
  cout << "Found " << interface_name << " as " << name << "\n";

#ifndef NDBUS
  auto ownership = try_claim_ownership( name );
#endif

  AudioPair uac2 { interface_name };
  uac2.initialize();

  EventLoop loop;

  uint64_t packet_counter = 0;
  uint64_t num_samples = 0;

  AudioBuffer audio_output { 65536 };

  double buffer = 0;
  uint64_t silent_packets = 0;
  bool first_packet_received = false;

  auto loopback_rule = loop.add_rule(
    "loopback",
    uac2.fd(),
    Direction::In,
    [&] {
      uac2.loopback( audio_output );

      if ( first_packet_received ) {
        num_samples += uac2.mic_avail();
        if ( num_samples >= SAMPLES_INTERVAL )
        {
          buffer -= static_cast<double>( num_samples ) / static_cast<double>( SAMPLE_RATE_MS );
          buffer_vals.push_back( buffer );
          // cout << buffer << endl;
          num_samples = 0;
        }
      }
    },
    [&] { return packet_counter < num_packets; },
    [] {},
    [&] {
      uac2.recover();
      return true;
    } );

  UDPSocket receive_sock;
  receive_sock.set_blocking( false );
  receive_sock.bind( { "0", 9090 } );
  Address server = { SOPHON_ADDR, 9090 };
  receive_sock.sendto( server, "hello" ); // TODO: timeout and resend b/c UDP unreliable

  auto receive_rule = loop.add_rule(
    "Receive packets",
    receive_sock,
    Direction::In,
    [&] {
      auto recv = receive_sock.recv();
      string payload = recv.payload;
      uint64_t packet_number = stoull( payload );

      if ( !first_packet_received ) {
        cout << "first packet received: " << packet_number << endl;
        packet_counter = packet_number + 1; // don't know when we'll get the first packet
        first_packet_received = true;
      }

      if ( packet_number >= packet_counter ) {
        buffer += ( packet_number - packet_counter + 1 ) * 2.5;
        for ( size_t i = packet_counter; i < packet_number; i++ ) {
          packets_received.push_back( 0 );
          silent_packets++;
        }
        packets_received.push_back( 1 );
        packet_counter = packet_number + 1;
      }
    },
    [&] { return packet_counter < num_packets; } );

  auto buffer_rule = loop.add_rule(
    "read from buffer",
    [&] {
      audio_output.ch1.pop( audio_output.ch1.num_stored() );
      audio_output.ch2.pop( audio_output.ch2.num_stored() );
    },
    [&] { return audio_output.ch1.num_stored() > 0 && packet_counter < num_packets; } );

  uac2.start();
  auto start_time = steady_clock::now();
  while ( loop.wait_next_event( -1 ) != EventLoop::Result::Exit ) {
  }
  cout << loop.summary() << "\n";
  auto end_time = steady_clock::now();
  duration<double, ratio<1, 1000>> total_time = end_time - start_time;
  cout << "TOTAL TIME: " << total_time.count() << " ms" << endl;
  cout << "BUFFER: " << buffer << " ms" << endl;
  cout << "# Silent packets: " << silent_packets << "("
       << static_cast<double>( silent_packets ) / static_cast<double>( num_packets ) << "%)" << endl;
}

/*Exports data about buffer sizes and packet drops*/
void export_data( vector<double>& buffer_vals, vector<int>& packets_received )
{
  cout << "EXPORTING" << endl;
  std::fstream fout;
  fout.open( BUFFER_CSV, std::ios::out );
  for ( size_t i = 0; i < buffer_vals.size(); i++ ) {
    fout << buffer_vals.at( i ) << "\n";
  }
  fout.close();

  fout.open( PACKET_CSV, std::ios::out );
  for ( size_t i = 0; i < packets_received.size(); i++ ) {
    fout << packets_received.at( i ) << "\n";
  }
  fout.close();
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

    vector<double> buffer_values;
    vector<int> packets_received;
    program_body( num_packets, buffer_values, packets_received );
    export_data( buffer_values, packets_received );
    cout << global_timer().summary() << "\n";
  } catch ( const exception& e ) {
    cout << "Exception: " << e.what() << endl;
    cout << global_timer().summary() << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
