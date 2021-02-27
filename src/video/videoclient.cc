#include "videoclient.hh"
#include "connection.hh"
#include "timestamp.hh"

using namespace std;
using namespace std::chrono;

VideoClient::NetworkSession::NetworkSession( const uint8_t node_id,
                                             const KeyPair& session_key,
                                             const Address& destination )
  : connection( node_id, 0, CryptoSession( session_key.uplink, session_key.downlink ), destination )
{}

void VideoClient::NetworkSession::transmit_frame( VideoSource& source, UDPSocket& socket )
{
  connection.push_frame( source );
  connection.send_packet( socket );
}

void VideoClient::NetworkSession::network_receive( const Ciphertext& ciphertext )
{
  connection.receive_packet( ciphertext );

  if ( connection.has_inbound_unreliable_data() ) {
    Parser p { connection.inbound_unreliable_data() };
    control.emplace();
    p.object( control.value() );
    if ( p.error() ) {
      p.clear_error();
    }
    connection.pop_inbound_unreliable_data();
  }
}

void VideoClient::NetworkSession::summary( std::ostream& out ) const
{
  connection.summary( out );
}

void VideoClient::process_keyreply( const Ciphertext& ciphertext )
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
    session_.emplace( keys.id, keys.key_pair, server_ );
    stats_.new_sessions++;
  } else {
    stats_.bad_packets++;
  }
}

VideoClient::VideoClient( const Address& server,
                          const LongLivedKey& key,
                          shared_ptr<VideoSource> source,
                          EventLoop& loop )
  : server_( server )
  , name_( key.name() )
  , long_lived_crypto_( key.key_pair().uplink, key.key_pair().downlink, true )
  , source_( source )
  , next_key_request_( steady_clock::now() )
{
  socket_.set_blocking( false );

  loop.add_rule(
    "network transmit",
    [&] {
      session_->transmit_frame( *source_, socket_ );
      if ( session_->connection.sender_stats().last_good_ack_ts + 4'000'000'000 < Timer::timestamp_ns() ) {
        stats_.timeouts++;
        session_.reset();
      }
    },
    [&] { return source_->ready( Timer::timestamp_ns() ) and session_.has_value(); } );

  loop.add_rule(
    "discard video",
    [&] { source_->pop_frame(); },
    [&] { return source_->has_frame() and not session_.has_value(); } );

  loop.add_rule( "network receive", socket_, Direction::In, [&] {
    Address src { nullptr, 0 };
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
            session_->network_receive( ciphertext );
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

  loop.add_rule(
    "key request",
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
}

void VideoClient::summary( ostream& out ) const
{
  out << "Peer [" << name_ << "]:";
  out << " key_requests=" << stats_.key_requests;
  out << " sessions=" << stats_.new_sessions;
  out << " bad_packets=" << stats_.bad_packets;
  out << " timeouts=" << stats_.timeouts << "\n";
  if ( session_.has_value() ) {
    session_->summary( out );
  }
}
