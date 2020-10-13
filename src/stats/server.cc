#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>
#include <unistd.h>
#include <thread>
#include <pthread.h>

#include "eventloop.hh"
#include "exception.hh"
#include "socket.hh"
#include "timer.hh"

using namespace std;

const string HOME(getenv("HOME"));

const string BUFFER_CSV = HOME + "/audio/csv/buffer_every_one_ms.csv";
const string PACKET_CSV = HOME + "/audio/csv/packets_received.csv";
const uint64_t NS_PER_MS { 1'000'000 };
const uint64_t DELAY { 2'500'000 };
const uint64_t MAX_NUM_PACKETS = 10;

void program_body( vector<int64_t>& buffer_vals, vector<int>& packets_received ) {
  (void)buffer_vals;
  (void)packets_received;
  EventLoop event_loop;
  UDPSocket server_sock;
  const uint64_t timeout = 1;
  server_sock.set_blocking( false );
  server_sock.bind( { "0", 9090 } );

  auto prev_time = chrono::steady_clock::now();
  atomic<uint64_t> server_packet_counter = 0;
  double buffer = 0;
  std::mutex mtx_buf_vec;
  std::mutex mtx_packet_vec;
  
  bool packet_zero_received = false;
  thread decrementer;
  double average_time = 0;
  event_loop.add_rule(
    "Server receive packets and keep track of buffer",
    server_sock,
    Direction::In,
    [&] {
      auto recv = server_sock.recv();
      string payload = recv.payload;
      uint64_t packet_number = stoull( payload );
      if (packet_number == 0) {
        buffer += 2.5; //TODO: MAGIC NUMBER
	buffer -= 1; //TODO: MAGIC NUMBER
        server_packet_counter++;
        unique_lock<mutex> lk(mtx_packet_vec);
        packets_received.push_back(1);
	packet_zero_received = true;
      } else if (packet_number >= server_packet_counter) {
        buffer += (server_packet_counter - packet_number + 1)*2.5;
        unique_lock<mutex> lk(mtx_packet_vec);
        for (size_t i = server_packet_counter; i < packet_number; i++) {
          packets_received.push_back(0);
        }
        packets_received.push_back(1);
        server_packet_counter = packet_number + 1;
      }
      //cout << "packet " << packet_number << "; buffer " << buffer << endl;
      unique_lock<mutex> lk(mtx_buf_vec);
      buffer_vals.push_back(buffer);
      
      /*Extra part of the event_loop to track the times as a consistency of check*/
      chrono::duration<double, ratio<1, 1000>> diff = chrono::steady_clock::now() - prev_time;
      if (packet_number > 0) { 
        average_time += diff.count();
      }
      prev_time = chrono::steady_clock::now();
    },
    [&] { return server_packet_counter < MAX_NUM_PACKETS; } );
  prev_time = chrono::steady_clock::now(); 
  while ( event_loop.wait_next_event(timeout) != EventLoop::Result::Exit ) {
    if (packet_zero_received) {
      buffer -= (timeout);
      cout << "Buffer: " << buffer << endl;
    }
  }
  average_time /= (MAX_NUM_PACKETS - 1);
  cout << "Average time: " << average_time << endl;
  cout << "Buffer: " << buffer << endl;
}

/*Exports data about buffer sizes and packet drops*/
void export_data( vector<int64_t>& buffer_vals, vector<int>& packets_received ) {
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

int main() {
  try {
    global_timer();
    vector<int64_t> buffer_values;
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
