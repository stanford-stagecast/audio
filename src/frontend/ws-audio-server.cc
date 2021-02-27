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
using namespace std::chrono;

struct EventCategories
{
  size_t close, SSL_read, SSL_write, ws_handshake, ws_receive, send_init_segment;

  EventCategories( EventLoop& loop )
    : close( loop.add_category( "close" ) )
    , SSL_read( loop.add_category( "SSL_read" ) )
    , SSL_write( loop.add_category( "SSL_write" ) )
    , ws_handshake( loop.add_category( "WebSocket handshake" ) )
    , ws_receive( loop.add_category( "WebSocket receive" ) )
    , send_init_segment( loop.add_category( "sent initial segment" ) )
  {}
};

class ClientConnection
{
  SSLSession ssl_session_;
  WebSocketServer ws_server_;
  string_view init_segment_;
  bool init_segment_sent_ {};

  vector<EventLoop::RuleHandle> rules_;

  bool good_ = true;
  string error_text_ {};

  shared_ptr<bool> cull_needed_;

  void cull( const string_view s )
  {
    *cull_needed_ = true;
    good_ = false;

    if ( not error_text_.empty() ) {
      error_text_ += " + "sv;
    }
    error_text_ += s;

    for ( auto& rule : rules_ ) {
      rule.cancel();
    }
    rules_.clear();
  }

public:
  const TCPSocket& socket() const { return ssl_session_.socket(); }

  void send_all( const string_view s ) { ws_server_.endpoint().send_all( s, ssl_session_.outbound_plaintext() ); }
  bool can_send( const size_t len ) const
  {
    return ssl_session_.outbound_plaintext().writable_region().size() >= len;
  }

  ClientConnection( const EventCategories& categories,
                    SSLContext& context,
                    TCPSocket& listening_socket,
                    const string& origin,
                    EventLoop& loop,
                    shared_ptr<bool> cull_needed,
                    const string_view init_segment )
    : ssl_session_( context.make_SSL_handle(),
                    [&] {
                      auto sock = listening_socket.accept();
                      sock.set_blocking( false );
                      sock.set_tcp_nodelay( true );
                      return sock;
                    }() )
    , ws_server_( origin )
    , init_segment_( init_segment )
    , rules_()
    , cull_needed_( cull_needed )
  {
    cerr << "New connection from " << ssl_session_.socket().peer_address().to_string() << "\n";

    rules_.reserve( 6 );

    rules_.push_back( loop.add_rule(
      categories.close,
      [this] { cull( "WebSocket closure or error" ); },
      [this] {
        return good() and ws_server_.should_close_connection()
               and ssl_session_.outbound_plaintext().readable_region().empty();
      } ) );

    rules_.push_back( loop.add_rule(
      categories.SSL_read,
      ssl_session_.socket(),
      Direction::In,
      [this] {
        try {
          ssl_session_.do_read();
        } catch ( const exception& e ) {
          cull( e.what() );
        }
      },
      [this] { return good() and ssl_session_.want_read(); },
      [this] { cull( "socket closed" ); } ) );

    rules_.push_back( loop.add_rule(
      categories.SSL_write,
      ssl_session_.socket(),
      Direction::Out,
      [this] {
        try {
          ssl_session_.do_write();
        } catch ( const exception& e ) {
          cull( e.what() );
        }
      },
      [this] { return good() and ssl_session_.want_write(); },
      [this] { cull( "socket closed" ); } ) );

    rules_.push_back( loop.add_rule(
      categories.ws_handshake,
      [this] { ws_server_.do_handshake( ssl_session_.inbound_plaintext(), ssl_session_.outbound_plaintext() ); },
      [this] {
        return good() and ( not ssl_session_.inbound_plaintext().readable_region().empty() )
               and ( not ws_server_.handshake_complete() );
      } ) );

    rules_.push_back( loop.add_rule(
      categories.send_init_segment,
      [this] {
        init_segment_sent_ = true;
        if ( can_send( init_segment_.size() ) ) {
          send_all( init_segment_ );
        } else {
          cull( "can't send initial segment" );
        }
      },
      [this] { return good() and ws_server_.handshake_complete() and not init_segment_sent_; } ) );

    rules_.push_back( loop.add_rule(
      categories.ws_receive,
      [this] {
        ws_server_.endpoint().read( ssl_session_.inbound_plaintext(), ssl_session_.outbound_plaintext() );
        if ( ws_server_.endpoint().ready() ) {
          cerr << "got message: " << ws_server_.endpoint().message() << "\n";
          ws_server_.endpoint().pop_message();
        }
      },
      [this] { return good() and not ssl_session_.inbound_plaintext().readable_region().empty(); } ) );
  }

  bool handshake_complete() const { return ws_server_.handshake_complete(); }

  ~ClientConnection()
  {
    if ( not error_text_.empty() ) {
      cerr << "Client error: " << error_text_ << "\n";
    }
  }

  SSLSession& session() { return ssl_session_; }

  bool good() const { return good_; }

  ClientConnection( const ClientConnection& other ) noexcept = delete;
  ClientConnection& operator=( const ClientConnection& other ) noexcept = delete;

  ClientConnection( ClientConnection&& other ) noexcept = delete;
  ClientConnection& operator=( ClientConnection&& other ) noexcept = delete;
};

void program_body( const string origin, const string cert_filename, const string privkey_filename )
{
  ios::sync_with_stdio( false );

  if ( SIG_ERR == signal( SIGPIPE, SIG_IGN ) ) {
    throw unix_error( "signal" );
  }

  SSLServerContext ssl_context { cert_filename, privkey_filename };

  /* read init segment */
  const ReadOnlyFile init_segment_file { "/tmp/stagecast-audio.init" };
  WebSocketFrame init_segment_frame;
  init_segment_frame.fin = true;
  init_segment_frame.opcode = WebSocketFrame::opcode_t::Binary;
  init_segment_frame.payload = init_segment_file;
  string init_segment;
  init_segment.resize( init_segment_frame.serialized_length() );
  init_segment_frame.serialize( init_segment );

  /* receive new additions to stream */
  UnixDatagramSocket stream_receiver;
  stream_receiver.set_blocking( false );
  stream_receiver.bind( Address::abstract_unix( "stagecast-program-audio" ) );

  /* start listening for HTTP connections */
  TCPSocket web_listen_socket;
  web_listen_socket.set_reuseaddr();
  web_listen_socket.set_blocking( false );
  web_listen_socket.set_tcp_nodelay( true );
  web_listen_socket.bind( { "0", 8081 } );
  web_listen_socket.listen();

  struct ClientList : public Summarizable
  {
    list<ClientConnection> clients {};

    virtual void summary( ostream& out ) const override
    {
      out << "Connections: ";
      unsigned int i = 0;
      auto it = clients.begin();
      while ( it != clients.end() ) {
        out << "   " << i << ":\t" << it->socket().peer_address().to_string() << "\n";
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
  EventCategories categories { *loop };

  auto cull_needed = make_shared<bool>( false );

  WebSocketFrame frame_segment;
  frame_segment.fin = true;
  frame_segment.opcode = WebSocketFrame::opcode_t::Binary;

  string frame_segment_serialized;

  loop->add_rule( "new audio segment", stream_receiver, Direction::In, [&] {
    frame_segment.payload.resize( 1048576 );
    frame_segment.payload.resize( stream_receiver.recv( string_span::from_view( frame_segment.payload ) ) );
    frame_segment.serialize( frame_segment_serialized );

    for ( auto& client : clients->clients ) {
      if ( client.handshake_complete() and client.can_send( frame_segment_serialized.length() ) ) {
        client.send_all( frame_segment_serialized );
      }
    }
  } );

  loop->add_rule( "new TCP connection", web_listen_socket, Direction::In, [&] {
    clients->clients.emplace_back(
      categories, ssl_context, web_listen_socket, origin, *loop, cull_needed, init_segment );
  } );

  /* cull old connections */
  loop->add_rule(
    "cull connections",
    [&] {
      clients->clients.remove_if( []( const ClientConnection& x ) { return not x.good(); } );
      *cull_needed = false;
    },
    [&cull_needed] { return *cull_needed; } );

  /*
  StatsPrinterTask stats_printer { loop };
  stats_printer.add( clients );
  */

  while ( loop->wait_next_event( 500 ) != EventLoop::Result::Exit ) {
  }
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }
    if ( argc != 4 ) {
      cerr << "Usage: " << argv[0] << " origin certificate private_key\n";
      return EXIT_FAILURE;
    }

    program_body( argv[1], argv[2], argv[3] );
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
