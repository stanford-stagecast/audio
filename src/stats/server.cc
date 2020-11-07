#include "address.hh"
#include "socket.hh"

#include <vector>
#include <map>
#include <mutex>

using namespace std;

mutex map_mtx;
map<Address, vector<received_datagram>> address_mapping;

void check_address(received_datagram dgram) {
  address_mapping[dgram.source_address].push_back(dgram);
}

void send_lingering_datagrams(UDPSocket intermediate) {
  for (map<Address, vector<received_datagram>>::iterator it = address_mapping.begin(); 
       it != address_mapping.end(); ++it) {
    vector<received_datagram> dgrams = address_mapping[it->first];
    for (map<Address, vector<received_datagram>>::iterator sendAddr = address_mapping.begin(); 
       sendAddr != address_mapping.end(); ++sendAddr)) {
      if (sendAddr->first == it->first) {
        continue;
      }
      intermediate.send_to(sendAddr->first, dgram.payload);
    }
  }
}

void pass_along_datagrams(UDPSocket& intermediate) {
  EventLoop loop;
  
  auto receive_loopback_rule = loop.add_rule(
    "wait to receive datagram",
    intermediate,
    Direction::In,
    [&] {
      auto datagram = intermediate.recv();
      map_mtx.lock();
      check_address(datagram);
      map_mtx.unlock();
    },
    [&] { return true; });

  auto send_loopback_rule = loop.add_rule(
    "send out datagram once received",
    intermediate,
    Direction::In, //TODO: Check
    [&] {
      map_mtx.lock();
      send_lingering_datagrams(intermediate);
      map_mtx.unlock();
    },
    [&] { return true; });

  loop.add_rule( "exit on keystroke", input, Direction::In, [&] {
    loopback_rule.cancel();
    input.close();
  } );

  while (loop.wait_next_event(-1) != EventLoop::Result::Exit) {
  }

  cout << loop.summary() << endl;
}

int main(void) {
  UDPSocket intermediate;
  intermediate.bind({"0", 9090}); //Default address
  pass_along_datagrams(intermediate);
  return 0;
}
