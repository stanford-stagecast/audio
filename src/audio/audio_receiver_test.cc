#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

#include "alsa_devices.hh"
#include "eventloop.hh"
#include "exception.hh"

using namespace std;
using namespace std::chrono;

pair<string, string> find_device( const string_view expected_description )
{
  ALSADevices devices;
  bool found = false;

  string name, interface_name;

  for ( const auto& dev : devices.list() ) {
    for ( const auto& interface : dev.interfaces ) {
      if ( interface.second == expected_description ) {
        if ( found ) {
          throw runtime_error( "Multiple devices matching description" );
        } else {
          found = true;
          name = dev.name;
          interface_name = interface.first;
        }
      }
    }
  }

  if ( not found ) {
    throw runtime_error( "Device \"" + string( expected_description ) + "\" not found" );
  }

  return { name, interface_name };
}

void program_body()
{
  ios::sync_with_stdio( false );

  EventLoop loop;
  UDPSocket socket;
  socket.set_blocking(false);
  socket.bind( {"0", 9090 });

  FileDescriptor input { CheckSystemCall( "dup STDIN_FILENO", dup( STDIN_FILENO ) ) };

  auto loopback_rule = loop.add_rule(
    "audio loopback",
    socket,
    Direction::In,
    [&] {
      auto recv = socket.recv();
      string payload = recv.payload;
      cout << payload << endl;
      // const char *bytes = payload.c_str();

      // int32_t sample_value;
      // memcpy(&sample_value, bytes, sizeof(int32_t));

      // cout << sample_value << endl;
    },
    [] { return true; } );

  loop.add_rule( "exit on keystroke", input, Direction::In, [&] {
    loopback_rule.cancel();
    input.close();
  } );

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
