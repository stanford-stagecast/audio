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

  string error_text {};

public:
  ClientConnection( SSLContext& context, TCPSocket& listening_socket, EventLoop& loop )
    : ssl_session_(
      context.make_SSL_handle(),
      [&] {
        auto sock = listening_socket.accept();
        sock.set_blocking( false );
        return sock;
      }(),
      "stagecast-backstage.keithw.org" )
    , rules_()
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
          good_ = false;
          error_text = e.what();
        }
      },
      [this] { return good_ and ssl_session_.want_read(); },
      [this] { good_ = false; } ) );
    rules_.push_back( loop.add_rule(
      "SSL write",
      ssl_session_.socket(),
      Direction::Out,
      [this] {
        try {
          ssl_session_.do_write();
        } catch ( const exception& e ) {
          good_ = false;
          error_text = e.what();
        }
      },
      [this] { return good_ and ssl_session_.want_write(); },
      [this] { good_ = false; } ) );
  }

  ~ClientConnection()
  {
    for ( auto& rule : rules_ ) {
      rule.cancel();
    }
    if ( not error_text.empty() ) {
      cerr << "Client error: " << error_text << "\n";
    }
  }

  bool good() const { return good_; }

  ClientConnection( ClientConnection&& other ) noexcept = default;
  ClientConnection& operator=( ClientConnection&& other ) noexcept = default;
};

void program_body()
{
  ios::sync_with_stdio( false );

  if ( SIG_ERR == signal( SIGPIPE, SIG_IGN ) ) {
    throw unix_error( "signal" );
  }

  SSLContext ssl_context;

  TCPSocket listen_socket;
  listen_socket.set_reuseaddr();
  listen_socket.set_blocking( false );
  listen_socket.bind( { "0", 8080 } );
  listen_socket.listen();

  /* set up event loop */
  EventLoop loop;

  /* accept new clients */
  vector<ClientConnection> clients;
  loop.add_rule( "accept TCP connection", listen_socket, Direction::In, [&] {
    clients.emplace_back( ssl_context, listen_socket, loop );
  } );

  /* cull old connections */
  auto next_cull = steady_clock::now() + seconds( 1 );
  loop.add_rule(
    "cull connections",
    [&] {
      cerr << "cull... ";
      auto it = clients.begin();
      while ( it != clients.end() ) {
        if ( it->good() ) {
          ++it;
        } else {
          it = clients.erase( it );
        }
      }
      next_cull = steady_clock::now() + seconds( 1 );
      cerr << "done.\n";
    },
    [&] { return steady_clock::now() > next_cull; } );

  while ( loop.wait_next_event( 250 ) != EventLoop::Result::Exit ) {
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
