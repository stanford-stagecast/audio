#include "eventloop.hh"
#include "exception.hh"
#include "socket.hh"
#include "timer.hh"
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <unistd.h>

using namespace std;

const uint64_t MAX_NUM_PACKETS = 100;
const uint64_t DELAY{ 1'000'000 };
Address server {"171.67.76.94", 9090};

string build_packet(int packet_counter)
{
  return std::to_string(packet_counter) + "12345678"; //40 byte packet
}

void program_body()
{
  EventLoop event_loop;
  UDPSocket client_sock;
  client_sock.set_blocking(false);
  uint64_t packet_counter = 1;
  while (packet_counter < MAX_NUM_PACKETS) {
    usleep(2500);
    cout << "Packet counter: " << packet_counter << endl;
    string packet_content = build_packet(packet_counter);
    client_sock.sendto(server, packet_content);
    packet_counter++;
  }
}

int main()
{
  try {
    global_timer();
    program_body();
    cout << global_timer().summary() << "\n";
  }
  catch (const exception& e) {
    cout << "Exception: " << e.what() << endl;
    cout << global_timer().summary() << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
