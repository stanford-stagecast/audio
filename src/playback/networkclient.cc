#include "networkclient.hh"
#include "timestamp.hh"

using namespace std;
using namespace std::chrono;

NetworkClient::NetworkSession::NetworkSession( const uint8_t node_id,
                                               const KeyPair& session_key,
                                               const Address& destination )
  : connection( node_id, 0, CryptoSession( session_key.uplink, session_key.downlink ), destination )
  , cursor( 960 )
{}

void NetworkClient::NetworkSession::transmit_frame( OpusEncoderProcess& source, UDPSocket& socket )
{
  connection.push_frame( source );
  connection.send_packet( socket );
}

void NetworkClient::NetworkSession::network_receive( const Ciphertext& ciphertext )
{
  connection.receive_packet( ciphertext );
}

void NetworkClient::NetworkSession::decode( const size_t decode_cursor,
                                            OpusDecoderProcess& decoder,
                                            ChannelPair& output )
{
  /* decode server's Opus frames to playback buffer */
  cursor.sample( connection.frames(),
                 decode_cursor,
                 connection.unreceived_beyond_this_frame_index() * opus_frame::NUM_SAMPLES_MINLATENCY,
                 connection.next_frame_needed() * opus_frame::NUM_SAMPLES_MINLATENCY,
                 decoder,
                 output );

  /* pop used Opus frames from server */
  connection.pop_frames( min( cursor.ok_to_pop( connection.frames() ),
                              connection.next_frame_needed() - connection.frames().range_begin() ) );
}

void NetworkClient::NetworkSession::summary( std::ostream& out ) const
{
  cursor.summary( out );
  connection.summary( out );
}

void NetworkClient::process_keyreply( const Ciphertext& ciphertext )
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

NetworkClient::NetworkClient( const Address& server,
                              const LongLivedKey& key,
                              shared_ptr<OpusEncoderProcess> source,
                              shared_ptr<AudioDeviceTask> dest,
                              EventLoop& loop )
  : server_( server )
  , name_( key.name() )
  , long_lived_crypto_( key.key_pair().uplink, key.key_pair().downlink, true )
  , source_( source )
  , dest_( dest )
  , next_key_request_( steady_clock::now() )
{
  socket_.set_blocking( false );

  loop.add_rule(
    "network transmit",
    [&] { session_->transmit_frame( *source_, socket_ ); },
    [&] { return source_->has_frame() and session_.has_value(); } );

  loop.add_rule(
    "discard audio",
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
    "decode",
    [&] {
      session_->decode( decode_cursor_, decoder_, dest_->playback() );
      decode_cursor_ += opus_frame::NUM_SAMPLES_MINLATENCY;

      if ( session_->connection.sender_stats().last_good_ack_ts + 4'000'000'000 < Timer::timestamp_ns() ) {
        stats_.timeouts++;
        session_.reset();
      }
    },
    [&] {
      return session_.has_value()
             and ( dest_->cursor() + opus_frame::NUM_SAMPLES_MINLATENCY + 60 >= decode_cursor_ );
    } );

  loop.add_rule(
    "play silence",
    [&] { decode_cursor_ += opus_frame::NUM_SAMPLES_MINLATENCY; },
    [&] {
      return ( !session_.has_value() )
             and ( dest_->cursor() + opus_frame::NUM_SAMPLES_MINLATENCY + 60 >= decode_cursor_ );
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

void NetworkClient::summary( ostream& out ) const
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

void NetworkClient::set_cursor_lag( const uint16_t num_samples )
{
  if ( session_.has_value() ) {
    session_->cursor.set_target_lag( num_samples );
    session_->cursor.reset();
  }
}
