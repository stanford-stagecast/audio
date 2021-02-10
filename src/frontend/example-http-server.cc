#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "eventloop.hh"
#include "secure_socket.hh"

using namespace std;
using namespace std::chrono;

class ClientConnection
{
  SSLSession ssl_session_;
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
  ClientConnection( SSLContext& context,
                    TCPSocket& listening_socket,
                    EventLoop& loop,
                    shared_ptr<bool> cull_needed )
    : ssl_session_(
      context.make_SSL_handle(),
      [&] {
        auto sock = listening_socket.accept();
        sock.set_blocking( false );
        return sock;
      }(),
      "stagecast-backstage.keithw.org" )
    , rules_()
    , cull_needed_( cull_needed )
  {
    rules_.reserve( 2 );
    rules_.push_back( loop.add_rule(
      "SSL read",
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
      "SSL write",
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
  }

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

void program_body( const string cert_filename, const string privkey_filename )
{
  ios::sync_with_stdio( false );

  if ( SIG_ERR == signal( SIGPIPE, SIG_IGN ) ) {
    throw unix_error( "signal" );
  }

  SSLServerContext ssl_context { cert_filename, privkey_filename };

  TCPSocket listen_socket;
  listen_socket.set_reuseaddr();
  listen_socket.set_blocking( false );
  listen_socket.bind( { "0", 8080 } );
  listen_socket.listen();

  /* set up event loop */
  EventLoop loop;

  auto cull_needed = make_shared<bool>( false );

  /* accept new clients */
  list<ClientConnection> clients;
  loop.add_rule( "accept TCP connection", listen_socket, Direction::In, [&] {
    clients.emplace_back( ssl_context, listen_socket, loop, cull_needed );
    clients.back().session().outbound_plaintext().push_from_const_str( "Hello, world.\n" );
  } );

  /* cull old connections */
  loop.add_rule(
    "cull connections",
    [&] {
      clients.remove_if( []( const ClientConnection& x ) { return not x.good(); } );
      *cull_needed = false;
    },
    [&cull_needed] { return *cull_needed; } );

  while ( loop.wait_next_event( 250 ) != EventLoop::Result::Exit ) {
  }
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }
    if ( argc != 3 ) {
      cerr << "Usage: " << argv[0] << " certificate private_key\n";
      return EXIT_FAILURE;
    }

    program_body( argv[1], argv[2] );
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
