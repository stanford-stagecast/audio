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
  socket.sendto( { "127.0.0.1", control_port() }, buf );
}

void program_body( const string& control, const string& value )
{
  ios::sync_with_stdio( false );

  if ( control == "cursor" ) {
    set_cursor_lag instruction;
    instruction.num_samples = stoi( value );
    send( instruction );
  } else if ( control == "gain" ) {
    set_loopback_gain instruction;
    instruction.gain = stof( value );
    send( instruction );
  }
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }

    if ( argc != 3 ) {
      cerr << "Usage: " << argv[0] << " cursor|gain value\n";
      return EXIT_FAILURE;
    }

    program_body( argv[1], argv[2] );
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
