#include "unistd.h"
#include <iostream>
#include <map>
#include <queue>
#include <vector>

#include "address.hh"
#include "eventloop.hh"
#include "exception.hh"
#include "socket.hh"

using namespace std;

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
      if ( payload == "hello" ) {
        receiver_address = datagram.source_address;
        cout << "receiver address received: " << receiver_address.value() << endl;
      } else if ( receiver_address.has_value() ) {
        intermediate.sendto( receiver_address.value(), payload );
      } else {
        cout << "received from sender but no receiver set" << endl;
      }
    },
    [&] { return true; } );

  FileDescriptor input { CheckSystemCall( "dup STDIN_FILENO", dup( STDIN_FILENO ) ) };
  loop.add_rule( "exit on keystroke", input, Direction::In, [&] {
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
