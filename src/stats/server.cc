#include <iostream>
#include <map>
#include <vector>
#include <queue>
#include "unistd.h"


#include "address.hh"
#include "eventloop.hh"
#include "exception.hh"
#include "socket.hh"

using namespace std;

// map<Address, vector<UDPSocket::received_datagram>> address_mapping;

// void check_address( UDPSocket::received_datagram dgram )
// {
//   address_mapping[dgram.source_address].push_back( dgram );
// }

// void send_lingering_datagrams( UDPSocket& intermediate )
// {
//   for ( map<Address, vector<UDPSocket::received_datagram>>::iterator it = address_mapping.begin();
//         it != address_mapping.end();
//         ++it ) {
//     vector<UDPSocket::received_datagram> dgrams = address_mapping[it->first];
//     for ( map<Address, vector<UDPSocket::received_datagram>>::iterator sendAddr = address_mapping.begin();
//           sendAddr != address_mapping.end();
//           ++sendAddr )
//       {
//         if ( sendAddr->first == it->first ) {
//           continue;
//         }
//         intermediate.sendto( sendAddr->first, dgram.payload );
//       }
//   }
// }

void pass_along_datagrams( UDPSocket& intermediate )
{
  EventLoop loop;
  optional<Address> receiver_address = {};

  auto forward_rule = loop.add_rule(
    "forward datagrams",
    intermediate,
    Direction::In,
    [&] {
      auto datagram = intermediate.recv();
      string payload = datagram.payload;
      if (payload == "hello") {
        receiver_address = datagram.source_address;
        cout << "receiver address received: " << receiver_address.value() << endl;
      }
      else if (receiver_address.has_value()) {
        intermediate.sendto(receiver_address.value(), payload);
      }
    },
    [&] { return true; }
  );

  // auto receive_loopback_rule = loop.add_rule(
  //   "wait to receive datagram",
  //   intermediate,
  //   Direction::In,
  //   [&] {
  //     auto datagram = intermediate.recv();
  //     map_mtx.lock();
  //     check_address( datagram );
  //     map_mtx.unlock();
  //   },
  //   [&] { return true; } );

  // auto send_loopback_rule = loop.add_rule(
  //   "send out datagram once received",
  //   intermediate,
  //   Direction::In, // TODO: Check
  //   [&] {
  //     send_lingering_datagrams( intermediate );
  //   },
  //   [&] { return true; } );

  FileDescriptor input { CheckSystemCall( "dup STDIN_FILENO", dup( STDIN_FILENO ) ) };
  loop.add_rule( "exit on keystroke", input, Direction::In, [&] {
    // receive_loopback_rule.cancel();
    // send_loopback_rule.cancel();
    forward_rule.cancel();
    input.close();
  } );

  while ( loop.wait_next_event( -1 ) != EventLoop::Result::Exit ) {
  }

  cout << loop.summary() << endl;
}

int main( void )
{
  UDPSocket intermediate;
  intermediate.bind( { "0", 9090 } ); // Default address
  pass_along_datagrams( intermediate );
  return 0;
}
