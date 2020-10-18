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

#include "eventloop.hh"
#include "exception.hh"
#include "socket.hh"
#include "timer.hh"

using namespace std;

const string HOME( getenv( "HOME" ) );

const string BUFFER_CSV = HOME + "/audio/csv/buffer100k.csv";
const string PACKET_CSV = HOME + "/audio/csv/packets100k.csv";
const uint64_t NS_PER_MS { 1'000'000 };
const uint64_t DELAY { 2'500'000 };
const uint64_t MAX_NUM_PACKETS = 100000;

void program_body( vector<double>& buffer_vals, vector<int>& packets_received )
{
  (void)buffer_vals;
  (void)packets_received;
  EventLoop event_loop;
  UDPSocket server_sock;
  const uint64_t timeout = 1;
  server_sock.set_blocking( false );
  server_sock.bind( { "0", 9090 } );

  auto prev_time = chrono::steady_clock::now();
  auto prev_decrement_time = chrono::steady_clock::now();
  uint64_t server_packet_counter = 0;
  double buffer = 0;
  bool packet_zero_received = false;
  double average_time = 0;

  event_loop.add_rule(
    "Server receive packets and keep track of buffer",
    server_sock,
    Direction::In,
    [&] {
      auto recv = server_sock.recv();
      string payload = recv.payload;
      uint64_t packet_number = stoull( payload );

      if ( packet_number == 0 ) {
        prev_decrement_time = chrono::steady_clock::now();
        server_packet_counter++;
        packets_received.push_back( 1 );
        packet_zero_received = true;
      } else if ( packet_number >= server_packet_counter ) {
        buffer += ( packet_number - server_packet_counter + 1 ) * 2.5;
        for ( size_t i = server_packet_counter; i < packet_number; i++ ) {
          packets_received.push_back( 0 );
        }
        packets_received.push_back( 1 );
        server_packet_counter = packet_number + 1;
      }

      buffer_vals.push_back( buffer );

      /*Extra part of the event_loop to track the times as a consistency of check*/
      chrono::duration<double, ratio<1, 1000>> diff = chrono::steady_clock::now() - prev_time;
      if ( packet_number > 0 ) {
        // cout << "diff: " << diff.count() << endl;
        average_time += diff.count();
      }
      prev_time = chrono::steady_clock::now();
    },
    [&] { return server_packet_counter < MAX_NUM_PACKETS; } );

  prev_time = chrono::steady_clock::now();

  while ( event_loop.wait_next_event( timeout ) != EventLoop::Result::Exit ) {
    if ( packet_zero_received ) {
      auto current_time = chrono::steady_clock::now();
      chrono::duration<double, ratio<1, 1000>> time_since_decrement = current_time - prev_decrement_time;
      // cout << "time since last decrement: " << time_since_decrement.count() << endl;
      buffer -= time_since_decrement.count();
      prev_decrement_time = current_time;
      // buffer_vals.push_back( buffer ); // if we want to store buffer on time values instead of per packet
      // cout << "Buffer: " << buffer << endl;
    }
  }
  average_time /= ( MAX_NUM_PACKETS - 1 );
  cout << "Average time: " << average_time << endl;
  cout << "Buffer: " << buffer << endl;
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

int main()
{
  try {
    global_timer();
    vector<double> buffer_values;
    vector<int> packets_received;
    program_body( buffer_values, packets_received );
    export_data( buffer_values, packets_received );
    cout << global_timer().summary() << "\n";
  } catch ( const exception& e ) {
    cout << "Exception: " << e.what() << endl;
    cout << global_timer().summary() << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
