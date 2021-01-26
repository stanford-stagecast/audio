#include <cstdlib>
#include <iostream>

#include "controller.hh"
#include "file_descriptor.hh"
#include "parser.hh"

using namespace std;

void program_body( const string& cursor_lag )
{
  ios::sync_with_stdio( false );

  UDPSocket socket;

  set_cursor_lag instruction;
  instruction.num_samples = stoi( cursor_lag );
  Plaintext buf;
  Serializer s { buf.mutable_buffer() };
  instruction.serialize( s );
  buf.resize( s.bytes_written() );
  socket.sendto( { "127.0.0.1", ClientController::control_port() }, buf );
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }

    if ( argc != 2 ) {
      cerr << "Usage: " << argv[0] << " cursor_lag\n";
      return EXIT_FAILURE;
    }

    program_body( argv[1] );
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
