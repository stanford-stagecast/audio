#include <cstdlib>
#include <iostream>

#include <csignal>

#include "eventloop.hh"
#include "ewma.hh"
#include "mmap.hh"
#include "mp4writer.hh"
#include "secure_socket.hh"
#include "socket.hh"
#include "stackbuffer.hh"
#include "stats_printer.hh"
#include "ws_server.hh"

using namespace std;
using namespace std::chrono;

struct EventCategories
{
  size_t close, SSL_read, SSL_write, ws_handshake, ws_receive, ws_send;

  EventCategories( EventLoop& loop )
    : close( loop.add_category( "close" ) )
    , SSL_read( loop.add_category( "SSL_read" ) )
    , SSL_write( loop.add_category( "SSL_write" ) )
    , ws_handshake( loop.add_category( "WebSocket handshake" ) )
    , ws_receive( loop.add_category( "WebSocket receive" ) )
    , ws_send( loop.add_category( "WebSocket send" ) )
  {}
};

class ClientConnection
{
  SSLSession ssl_session_;
  WebSocketServer ws_server_;
  MP4Writer muxer_ { 24, 1280, 720 };
  WebSocketFrame ws_frame_;

  uint32_t video_frame_count_ {};

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

  float mean_buffer_ = 0.0;
  float last_buffer_ = 0.0;
  void parse_message( const string_view s )
  {
    if ( ( s.size() > 7 ) and ( s.substr( 0, 7 ) == "buffer " ) ) {
      string_view num = s.substr( 8 );
      last_buffer_ = stof( string( num ) );
      ewma_update( mean_buffer_, last_buffer_, 0.05 );
    }
  }

  bool skipping_ {};
  unsigned int frames_since_idr_ {};
  unsigned int idrs_since_skip_ {};
  uint64_t next_status_update_ {};

public:
  const TCPSocket& socket() const { return ssl_session_.socket(); }

  bool good() const { return good_; }

  void push_NAL( const string_view s )
  {
    if ( MP4Writer::is_idr( s ) ) {
      frames_since_idr_ = 0;
      skipping_ = false;
      idrs_since_skip_++;
    } else {
      frames_since_idr_++;
    }

    if ( skipping_ and frames_since_idr_ >= 47 ) {
      idrs_since_skip_ = 0;
    } else {
      muxer_.write( s, video_frame_count_, video_frame_count_ );
      video_frame_count_++;
    }
  }

  bool can_send( const size_t len ) const
  {
    return ssl_session_.outbound_plaintext().writable_region().size() >= len;
  }

  ClientConnection( const EventCategories& categories,
                    SSLContext& context,
                    TCPSocket& listening_socket,
                    const string& origin,
                    EventLoop& loop,
                    shared_ptr<bool> cull_needed )
    : ssl_session_( context.make_SSL_handle(),
                    [&] {
                      auto sock = listening_socket.accept();
                      sock.set_blocking( false );
                      sock.set_tcp_nodelay( true );
                      return sock;
                    }() )
    , ws_server_( origin )
    , ws_frame_()
    , rules_()
    , cull_needed_( cull_needed )
  {
    cerr << "New connection from " << ssl_session_.socket().peer_address().to_string() << "\n";

    rules_.reserve( 10 );

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

    ws_frame_.fin = true;
    ws_frame_.opcode = WebSocketFrame::opcode_t::Binary;

    rules_.push_back( loop.add_rule(
      categories.ws_send,
      [this] {
        ws_frame_.payload.resize(
          min( muxer_.output().readable_region().size() + 1,
               ssl_session_.outbound_plaintext().writable_region().size() - WebSocketFrame::max_overhead() - 1 ) );
        ws_frame_.payload.at( 0 ) = 0;
        memcpy(
          ws_frame_.payload.data() + 1, muxer_.output().readable_region().data(), ws_frame_.payload.size() - 1 );

        muxer_.output().pop( ws_frame_.payload.size() - 1 );

        Serializer s { ssl_session_.outbound_plaintext().writable_region() };
        s.object( ws_frame_ );
        ssl_session_.outbound_plaintext().push( s.bytes_written() );
      },
      [&] {
        return ssl_session_.outbound_plaintext().writable_region().size() > ( 1 + WebSocketFrame::max_overhead() )
               and muxer_.output().readable_region().size() and ws_server_.handshake_complete();
      } ) );

    rules_.push_back( loop.add_rule(
      categories.ws_send,
      [this] {
        const auto now = Timer::timestamp_ns();
        ws_frame_.payload = " time = " + to_string( now );
        ws_frame_.payload.at( 0 ) = 1;

        next_status_update_ = now + BILLION;

        Serializer s { ssl_session_.outbound_plaintext().writable_region() };
        s.object( ws_frame_ );
        ssl_session_.outbound_plaintext().push( s.bytes_written() );
      },
      [&] {
        return ws_server_.handshake_complete() and Timer::timestamp_ns() > next_status_update_
               and ssl_session_.outbound_plaintext().writable_region().size() > 100;
      } ) );

    rules_.push_back( loop.add_rule(
      categories.ws_receive,
      [this] {
        ws_server_.endpoint().read( ssl_session_.inbound_plaintext(), ssl_session_.outbound_plaintext() );
        if ( ws_server_.endpoint().ready() ) {
          parse_message( ws_server_.endpoint().message() );

          if ( last_buffer_ > 0.11 and mean_buffer_ > 0.11 and idrs_since_skip_ > 0 ) {
            skipping_ = true;
          }

          //          cerr << skipping_ << " " << idrs_since_skip_ << " " << last_buffer_ << " " << mean_buffer_
          //          <<
          //          "\n";

          ws_server_.endpoint().pop_message();
        }
      },
      [this] { return good() and ssl_session_.inbound_plaintext().readable_region().size(); } ) );
  }

  ~ClientConnection()
  {
    if ( not error_text_.empty() ) {
      cerr << "Client error: " << error_text_ << "\n";
    }
  }

  SSLSession& session() { return ssl_session_; }

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

  /* receive new additions to stream */
  UDPSocket stream_receiver;
  stream_receiver.set_blocking( false );
  stream_receiver.bind( { "127.0.0.1", 9115 } );

  /* start listening for HTTP connections */
  TCPSocket web_listen_socket;
  web_listen_socket.set_reuseaddr();
  web_listen_socket.set_blocking( false );
  web_listen_socket.set_tcp_nodelay( true );
  web_listen_socket.bind( { "0", 8400 } );
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

  /* set up event loop */
  auto loop = make_shared<EventLoop>();
  EventCategories categories { *loop };

  auto cull_needed = make_shared<bool>( false );

  Address src { nullptr, 0 };
  StackBuffer<0, uint16_t, 65535> buf;

  loop->add_rule( "new video segment", stream_receiver, Direction::In, [&] {
    buf.resize( stream_receiver.recv( src, buf.mutable_buffer() ) );

    for ( auto& client : clients->clients ) {
      client.push_NAL( buf );
    }
  } );

  loop->add_rule( "new TCP connection", web_listen_socket, Direction::In, [&] {
    clients->clients.emplace_back( categories, ssl_context, web_listen_socket, origin, *loop, cull_needed );
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
