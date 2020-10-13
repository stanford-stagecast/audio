#include <atomic>
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
const uint64_t MAX_NUM_PACKETS = 100;

void program_body( vector<int64_t>& buffer_vals, vector<int>& packets_received )
{
  (void)buffer_vals;
  (void)packets_received;
  EventLoop event_loop;
  UDPSocket server_sock;
  server_sock.set_blocking( false );
  server_sock.bind( { "0", 9090 } );

  // uint64_t start_time = 0;
  uint64_t prev_time = 0;
  uint64_t current_time = 0;
  atomic<uint64_t> server_packet_counter = 0;
  atomic<int64_t> buffer = 0;
  std::mutex mtx_buf_vec;
  std::mutex mtx_packet_vec;
  
  bool buffer_decrementing = true;
  thread decrementer;

  event_loop.add_rule(
    "Server receive packets and keep track of buffer",
    server_sock,
    Direction::In,
    [&] {
      auto recv = server_sock.recv();
      string payload = recv.payload;
      uint64_t packet_number = stoull( payload );
      
      current_time = Timer::timestamp_ns();
      if (prev_time > 0) {
        uint64_t elapsed = (current_time - prev_time);
        cout << "time since last packet: " << (elapsed) / NS_PER_MS << " ms" << endl;
      }
      else { // spawn decrementer on first packet
        decrementer = thread([&buffer_decrementing, &buffer] {
          while (buffer_decrementing) {
            usleep(2500);
            buffer--;
            cout << "decrement" << endl;
          }
        });
      }
      prev_time = current_time;

      if (packet_number == server_packet_counter) {
        if (packet_number != 0) // don't decrement buffer on first packet arrival
          buffer++;
        server_packet_counter++;

        unique_lock<mutex> lk(mtx_packet_vec);
        packets_received.push_back(1);
      }
      else if (packet_number > server_packet_counter) {
        cout << "SKIPPING PACKETS " << server_packet_counter << " to " << packet_number - 1 << endl;
        buffer += (server_packet_counter - packet_number) + 1;
        unique_lock<mutex> lk(mtx_packet_vec);
        for (size_t i = server_packet_counter; i < packet_number; ++i) {
          packets_received.push_back(0);
        }
        packets_received.push_back(1);
        server_packet_counter = packet_number + 1;
      }
      cout << "packet " << packet_number << "; buffer " << buffer << endl;
      unique_lock<mutex> lk(mtx_buf_vec);
      buffer_vals.push_back(buffer);
    },
    [&] { return server_packet_counter < MAX_NUM_PACKETS; } );

  while ( event_loop.wait_next_event( 5 ) != EventLoop::Result::Exit ) {
    /*Nothing in here*/
  }
  buffer_decrementing = false;
  decrementer.join();
}

/*Exports data about buffer sizes and packet drops*/
void export_data( vector<int64_t>& buffer_vals, vector<int>& packets_received )
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
