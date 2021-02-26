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
  socket.sendto( { "127.0.0.1", video_server_control_port() }, buf );
}

void program_body( const string& control, const string& name, const string& value __attribute( ( unused ) ) )
{
  ios::sync_with_stdio( false );

  if ( control == "live" ) {
    set_live instruction;
    instruction.name = NetString( name );
    send( instruction );
  } else {
    throw runtime_error( "unknown control" );
  }
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }

    if ( argc != 4 ) {
      cerr << "Usage: " << argv[0] << " live name\n";
      return EXIT_FAILURE;
    }

    program_body( argv[1], argv[2], argv[3] );
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
