#include <cstdlib>
#include <iostream>

#include <csignal>

#include "eventloop.hh"
#include "mmap.hh"
#include "secure_socket.hh"
#include "socket.hh"
#include "stackbuffer.hh"
#include "stats_printer.hh"
#include "ws_server.hh"

using namespace std;

void program_body()
{
  ios::sync_with_stdio( false );

  if ( SIG_ERR == signal( SIGPIPE, SIG_IGN ) ) {
    throw unix_error( "signal" );
  }

  FileDescriptor output { CheckSystemCall( "dup", dup( STDOUT_FILENO ) ) };

  /* read init segment */
  const ReadOnlyFile init_segment_file { "/tmp/stagecast-video.init" };

  output.write( init_segment_file );

  /* receive new additions to stream */
  UDPSocket stream_receiver;
  stream_receiver.set_blocking( false );
  stream_receiver.bind( { "127.0.0.1", 9016 } );

  /* set up event loop */
  auto loop = make_shared<EventLoop>();

  Address src;
  StackBuffer<0, uint16_t, 65535> datagram;

  loop->add_rule( "new video segment", stream_receiver, Direction::In, [&] {
    datagram.resize( stream_receiver.recv( src, datagram.mutable_buffer() ) );
    output.write( datagram );
  } );

  while ( loop->wait_next_event( 500 ) != EventLoop::Result::Exit ) {
  }
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }
    if ( argc != 1 ) {
      cerr << "Usage: " << argv[0] << "\n";
      return EXIT_FAILURE;
    }

    program_body();
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
