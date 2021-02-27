#include <cstdlib>
#include <iostream>

#include <csignal>

#include "control_messages.hh"
#include "eventloop.hh"
#include "ewma.hh"
#include "mmap.hh"
#include "secure_socket.hh"
#include "socket.hh"
#include "stackbuffer.hh"
#include "stats_printer.hh"
#include "webmwriter.hh"
#include "ws_server.hh"

using namespace std;
using namespace std::chrono;

template<class Message>
void send_control( const Message& message )
{
  StackBuffer<0, uint8_t, 255> buf;
  Serializer s { buf.mutable_buffer() };
  s.integer( Message::id );
  s.object( message );
  buf.resize( s.bytes_written() );

  UDPSocket socket;
  socket.sendto( { "127.0.0.1", server_control_port() }, buf );
}

void split_on_char( const string_view str, const char ch_to_find, vector<string_view>& ret )
{
  ret.clear();

  bool in_double_quoted_string = false;
  unsigned int field_start = 0; // start of next token
  for ( unsigned int i = 0; i < str.size(); i++ ) {
    const char ch = str[i];
    if ( ch == '"' ) {
      in_double_quoted_string = !in_double_quoted_string;
    } else if ( in_double_quoted_string ) {
      continue;
    } else if ( ch == ch_to_find ) {
      ret.emplace_back( str.substr( field_start, i - field_start ) );
      field_start = i + 1;
    }
  }

  ret.emplace_back( str.substr( field_start ) );
}

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
  WebSocketFrame ws_frame_;

  vector<EventLoop::RuleHandle> rules_;

  bool good_ = true;
  string error_text_ {};

  shared_ptr<bool> cull_needed_;

public:
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

private:
  vector<string_view> fields_ {};
  void parse_message( const string_view s )
  {
    split_on_char( s, ':', fields_ );

    if ( fields_.size() != 4 ) {
      return;
    }

    if ( fields_[0] == "gain" ) {
      const string_view board_name = fields_[1];
      const string_view channel_name = fields_[2];
      const string_view db_gain = fields_[3];

      try {
        const float absolute_gain = dbfs_to_float( stof( string( db_gain ) ) );
        set_gain instruction;
        instruction.board_name = board_name;
        instruction.channel_name = channel_name;
        instruction.gain1 = absolute_gain;
        instruction.gain2 = absolute_gain;
        send_control( instruction );
      } catch ( const exception& e ) {
      }
    }
  }

public:
  const TCPSocket& socket() const { return ssl_session_.socket(); }

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
      categories.ws_receive,
      [this] {
        ws_server_.endpoint().read( ssl_session_.inbound_plaintext(), ssl_session_.outbound_plaintext() );
        if ( ws_server_.endpoint().ready() ) {
          parse_message( ws_server_.endpoint().message() );

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

  void push_update( const string_view str )
  {
    if ( handshake_complete()
         and ssl_session_.outbound_plaintext().writable_region().size()
               > str.size() + WebSocketFrame::max_overhead() ) {
      ws_frame_.payload = str;

      Serializer s { ssl_session_.outbound_plaintext().writable_region() };
      s.object( ws_frame_ );
      ssl_session_.outbound_plaintext().push( s.bytes_written() );
    }
  }

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
  UnixDatagramSocket stream_receiver;
  stream_receiver.set_blocking( false );
  stream_receiver.bind( Address::abstract_unix( "stagecast-server-audio-json" ) );

  /* start listening for HTTP connections */
  TCPSocket web_listen_socket;
  web_listen_socket.set_reuseaddr();
  web_listen_socket.set_blocking( false );
  web_listen_socket.set_tcp_nodelay( true );
  web_listen_socket.bind( { "0", 8500 } );
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

  StackBuffer<0, uint32_t, 1048576> buf;

  loop->add_rule( "new update", stream_receiver, Direction::In, [&] {
    buf.resize( stream_receiver.recv( buf.mutable_buffer() ) );
    if ( buf.length() == 0 ) {
      return;
    }

    for ( auto& client : clients->clients ) {
      try {
        client.push_update( buf );
      } catch ( const exception& e ) {
        client.cull( "Muxer exception" );
      }
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

  StatsPrinterTask stats_printer { loop };
  stats_printer.add( clients );

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
