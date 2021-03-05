#include "connection.hh"
#include "eventloop.hh"
#include "keys.hh"
#include "mmap.hh"
#include "ring_buffer.hh"
#include "socket.hh"
#include "stackbuffer.hh"
#include "stats_printer.hh"

#include <chrono>
#include <iostream>
#include <unistd.h>

using namespace std;
using namespace std::chrono;

class SimpleConnection : public Summarizable
{
  string name_;
  UDPSocket socket_ {};
  Address server_;
  CryptoSession long_lived_crypto_;
  std::chrono::steady_clock::time_point next_key_request_ = steady_clock::now();

  struct MiniSession
  {
    char node_id;

    CryptoSession crypto;
  };

  optional<MiniSession> session_ {};

  struct Statistics
  {
    unsigned int key_requests, bad_packets, new_sessions, decryption_failures, buffer_full;
  } stats_ {};

  TypedRingBuffer<Plaintext> packets_received_ { 256 };

  void process_keyreply( const Ciphertext& ciphertext )
  {
    /* decrypt */
    Plaintext plaintext;
    if ( long_lived_crypto_.decrypt( ciphertext, { &KeyMessage::keyreq_server_id, 1 }, plaintext ) ) {
      Parser p { plaintext };
      KeyMessage keys;
      p.object( keys );
      if ( p.error() ) {
        stats_.bad_packets++;
        p.clear_error();
        return;
      }
      session_.emplace(
        MiniSession { char( keys.id ), CryptoSession { keys.key_pair.uplink, keys.key_pair.downlink } } );
      stats_.new_sessions++;
      cerr << name_ << ": started session as id " << keys.id << "\n";
    } else {
      stats_.bad_packets++;
    }
  }

  void session_receive( const Ciphertext& ciphertext )
  {
    if ( not session_.has_value() ) {
      throw runtime_error( "no session" );
    }

    if ( not packets_received_.writable_region().size() ) {
      stats_.buffer_full++;
      return;
    }

    /* decrypt */
    Plaintext& plaintext = packets_received_.writable_region().at( 0 );
    static constexpr char peer_id = 0;
    if ( not session_->crypto.decrypt( ciphertext, { &peer_id, 1 }, plaintext ) ) {
      stats_.decryption_failures++;
      return;
    }

    packets_received_.push( 1 );
  }

public:
  const string& name() const { return name_; }
  void send( const Plaintext& plaintext )
  {
    if ( not session_.has_value() ) {
      throw runtime_error( "no session" );
    }

    Ciphertext ciphertext;
    session_->crypto.encrypt( { &session_->node_id, 1 }, plaintext, ciphertext );
    socket_.sendto( server_, ciphertext );
  }

  bool has_session() const { return session_.has_value(); }

  const Plaintext& front() { return packets_received_.readable_region().at( 0 ); }
  bool has_packet() const { return packets_received_.readable_region().size() > 0; }
  void pop_packet() { packets_received_.pop( 1 ); }

  SimpleConnection( const LongLivedKey& key, const Address& server, EventLoop& loop )
    : name_( key.name() )
    , server_( server )
    , long_lived_crypto_( key.key_pair().uplink, key.key_pair().downlink, true )
  {
    loop.add_rule(
      "key request [" + name_ + "]",
      [&] {
        next_key_request_ = steady_clock::now() + milliseconds( 250 );
        Plaintext empty;
        empty.resize( 0 );
        Ciphertext keyreq;
        long_lived_crypto_.encrypt( { &KeyMessage::keyreq_id, 1 }, empty, keyreq );
        socket_.sendto( server_, keyreq );
        stats_.key_requests++;
      },
      [&] { return ( !session_.has_value() ) and ( next_key_request_ < steady_clock::now() ); } );

    loop.add_rule( "network receive [" + name_ + "]", socket_, Direction::In, [&] {
      Address src;
      Ciphertext ciphertext;
      ciphertext.resize( socket_.recv( src, ciphertext.mutable_buffer() ) );
      if ( ciphertext.length() > 24 ) {
        const uint8_t node_id = ciphertext.as_string_view().back();
        switch ( node_id ) {
          case uint8_t( KeyMessage::keyreq_server_id ):
            if ( not session_.has_value() ) {
              process_keyreply( ciphertext );
            }
            break;
          case 0:
            if ( session_.has_value() ) {
              session_receive( ciphertext );
            }
            break;
          default:
            stats_.bad_packets++;
            break;
        }
      } else {
        stats_.bad_packets++;
      }
    } );
  }

  void summary( ostream& out ) const override
  {
    out << "keyreqs=" << stats_.key_requests;
    out << " bad=" << stats_.bad_packets;
    out << " new_sessions=" << stats_.new_sessions;
    out << " decrypt_failures=" << stats_.decryption_failures;
    out << " buffer_full=" << stats_.buffer_full;
    out << "\n";
  }
};

int main( int argc, char* argv[] )
{
  if ( argc <= 0 ) {
    abort();
  }

  if ( argc != 7 ) {
    cerr << "Usage: " << argv[0] << " host1 service1 key1 host2 service2 key2\n";
    return EXIT_FAILURE;
  }

  ios::sync_with_stdio( false );

  ReadOnlyFile key1file { argv[3] };
  Parser p1 { key1file };
  LongLivedKey key1 { p1 };

  ReadOnlyFile key2file { argv[6] };
  Parser p2 { key2file };
  LongLivedKey key2 { p2 };

  auto loop = make_shared<EventLoop>();

  auto connection1 = make_shared<SimpleConnection>( key1, Address( argv[1], argv[2] ), *loop );
  auto connection2 = make_shared<SimpleConnection>( key2, Address( argv[4], argv[5] ), *loop );

  StatsPrinterTask stats { loop };
  stats.add( connection1 );
  stats.add( connection2 );

  loop->add_rule(
    "relay " + connection1->name() + "->" + connection2->name(),
    [&] {
      connection2->send( connection1->front() );
      connection1->pop_packet();
    },
    [&] { return connection1->has_packet() and connection2->has_session(); } );

  loop->add_rule(
    "relay " + connection2->name() + "->" + connection1->name(),
    [&] {
      connection1->send( connection2->front() );
      connection2->pop_packet();
    },
    [&] { return connection2->has_packet() and connection1->has_session(); } );

  /* make key requests */

  while ( loop->wait_next_event( -1 ) != EventLoop::Result::Exit ) {
  }

  return EXIT_SUCCESS;
}
