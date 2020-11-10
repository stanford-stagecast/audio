#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <unistd.h>
#include <vector>

#include "address.hh"
#include "eventloop.hh"
#include "exception.hh"
#include "socket.hh"

using namespace std;

void pass_along_datagrams( UDPSocket& intermediate )
{
  EventLoop loop;
  set<Address> client_addresses;

  auto forward_rule = loop.add_rule(
    "forward datagrams",
    intermediate,
    Direction::In,
    [&] {
      auto datagram = intermediate.recv();
      Address source_address = datagram.source_address;
      string payload = datagram.payload;

      if ( payload == "hello" ) {
        if ( !client_addresses.count( source_address ) ) {
          cout << "new client connected from Address: " << source_address << endl;
        }
        client_addresses.insert( source_address );
      } else {
        uint64_t packet_number = stoull( payload );
        if ( packet_number % 1000 == 0 ) {
          cout << "Source: " << source_address << " | Packet #" << packet_number << endl;
        }

        if ( !client_addresses.empty() ) {
          for ( Address addr : client_addresses ) {
            if ( addr != source_address ) {
              intermediate.sendto( addr, payload );
            }
          }
        }
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
