#include <cstdlib>
#include <iostream>

#include "control_messages.hh"
#include "file_descriptor.hh"
#include "parser.hh"
#include "socket.hh"
#include "stackbuffer.hh"

using namespace std;

template<class Message>
void send( const Message& message )
{
  StackBuffer<0, uint8_t, 255> buf;
  Serializer s { buf.mutable_buffer() };
  s.integer( Message::id );
  s.object( message );
  buf.resize( s.bytes_written() );

  UDPSocket socket;
  socket.sendto( { "127.0.0.1", client_control_port() }, buf );
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }

    if ( argc < 2 ) {
      cerr << "Usage: " << argv[0] << " cursor|gain ...\n";
      return EXIT_FAILURE;
    }

    if ( argv[1] == "cursor"s ) {
      if ( argc != 5 ) {
        throw runtime_error( "bad usage" );
      }
      set_cursor_lag instruction;
      instruction.target_samples = stoi( argv[2] );
      instruction.min_samples = stoi( argv[3] );
      instruction.max_samples = stoi( argv[4] );
      send( instruction );
    } else if ( argv[1] == "gain"s ) {
      if ( argc != 3 ) {
        throw runtime_error( "bad usage" );
      }
      set_gain instruction;
      instruction.gain1 = stof( argv[2] );
      send( instruction );
    } else {
      throw runtime_error( "unknown control" );
    }
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
