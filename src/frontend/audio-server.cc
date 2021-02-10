#include <cstdlib>
#include <iostream>

#include <csignal>

#include "eventloop.hh"
#include "mmap.hh"
#include "secure_socket.hh"
#include "socket.hh"
#include "stackbuffer.hh"
#include "stats_printer.hh"

using namespace std;

void program_body()
{
  ios::sync_with_stdio( false );

  /* read init segment */
  ReadOnlyFile init_segment { "/tmp/stagecast-audio.init" };

  /* receive new additions to stream */
  UDPSocket stream_receiver;
  stream_receiver.set_blocking( false );
  stream_receiver.bind( { "127.0.0.1", 9014 } );
  StackBuffer<0, uint16_t, 65535> most_recent_audio_frame;

  string header
    = "HTTP/1.1 200 OK\r\nContent-type: audio/webm; codecs=\"opus\"\r\nAccess-Control-Allow-Origin: *\r\n\r\n";

  /* start listening for HTTP connections */
  TCPSocket web_listen_socket;
  web_listen_socket.set_reuseaddr();
  web_listen_socket.set_blocking( false );
  web_listen_socket.bind( { "0", 8080 } );
  web_listen_socket.listen();

  struct ClientList : public Summarizable
  {
    std::list<TCPSocket> sockets {};

    virtual void summary( ostream& out ) const override
    {
      out << "Connections: ";
      unsigned int i = 0;
      auto it = sockets.begin();
      while ( it != sockets.end() ) {
        out << "   " << i << ":\t" << it->peer_address().to_string() << "\n";
        i++;
        it++;
      }
    }
  };
  auto clients = make_shared<ClientList>();

  if ( SIG_ERR == signal( SIGPIPE, SIG_IGN ) ) {
    throw unix_error( "signal" );
  }

  /* set up event loop */
  auto loop = make_shared<EventLoop>();

  Address src { nullptr, 0 };
  loop->add_rule( "new audio segment", stream_receiver, Direction::In, [&] {
    most_recent_audio_frame.resize( stream_receiver.recv( src, most_recent_audio_frame.mutable_buffer() ) );

    for ( auto it = clients->sockets.begin(); it != clients->sockets.end(); ) {
      try {
        it->write( most_recent_audio_frame.as_string_view() );
        ++it;
      } catch ( const exception& e ) {
        it = clients->sockets.erase( it );
      }
    }
  } );

  loop->add_rule( "new TCP connection", web_listen_socket, Direction::In, [&] {
    TCPSocket client_socket = web_listen_socket.accept();
    client_socket.set_blocking( false );

    client_socket.write( header );
    client_socket.write( init_segment );
    clients->sockets.push_back( move( client_socket ) );
  } );

  StatsPrinterTask stats_printer { loop };
  stats_printer.add( clients );

  while ( loop->wait_next_event( 500 ) != EventLoop::Result::Exit ) {
  }
}

int main()
{
  try {
    program_body();
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
