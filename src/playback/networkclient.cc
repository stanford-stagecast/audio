#include "networkclient.hh"
#include "timestamp.hh"

using namespace std;
using namespace std::chrono;

using Option = RubberBand::RubberBandStretcher::Option;

NetworkClient::NetworkSession::NetworkSession( const uint8_t node_id,
                                               const KeyPair& session_key,
                                               const Address& destination )
  : connection( node_id, 0, CryptoSession( session_key.uplink, session_key.downlink ), destination )
  , cursor( 960, 120, 1920 )
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
                                            RubberBand::RubberBandStretcher& stretcher,
                                            ChannelPair& output )
{
  /* decode server's Opus frames to playback buffer */
  const size_t frontier_sample_index = connection.unreceived_beyond_this_frame_index() * opus_frame::NUM_SAMPLES;

  cursor.setup( decode_cursor, frontier_sample_index );

  Cursor::AudioSlice audio;

  while ( cursor.initialized() and decode_cursor > cursor.num_samples_output() ) {
    cursor.sample( connection.frames(), frontier_sample_index, decoder, stretcher, audio );

    if ( audio.good ) {
      output.ch1().region( audio.sample_index, audio.length ).copy( audio.ch1_span() );
      output.ch2().region( audio.sample_index, audio.length ).copy( audio.ch2_span() );
    }
  }

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
  , stretcher_( 48000,
                2,
                Option::OptionProcessRealTime | Option::OptionThreadingNever | Option::OptionPitchHighConsistency
                  | Option::OptionWindowShort )
  , source_( source )
  , dest_( dest )
  , next_key_request_( steady_clock::now() )
{
  socket_.set_blocking( false );
  stretcher_.setMaxProcessSize( opus_frame::NUM_SAMPLES );
  stretcher_.calculateStretch();

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
      session_->decode( decode_cursor_, decoder_, stretcher_, dest_->playback() );
      decode_cursor_ += opus_frame::NUM_SAMPLES;

      if ( session_->connection.sender_stats().last_good_ack_ts + 4'000'000'000 < Timer::timestamp_ns() ) {
        stats_.timeouts++;
        session_.reset();
      }
    },
    [&] { return session_.has_value() and ( dest_->cursor() + opus_frame::NUM_SAMPLES + 60 >= decode_cursor_ ); } );

  loop.add_rule(
    "play silence",
    [&] { decode_cursor_ += opus_frame::NUM_SAMPLES; },
    [&] {
      return ( !session_.has_value() ) and ( dest_->cursor() + opus_frame::NUM_SAMPLES + 60 >= decode_cursor_ );
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

void NetworkClient::json_summary( Json::Value& root ) const
{
  if ( session_.has_value() ) {
    session_->json_summary( root[name_]["client"]["feed"] );
  } else {
    Cursor::default_json_summary( root[name_]["client"]["feed"] );
  }
}

void NetworkClient::set_cursor_lag( const uint16_t target_samples,
                                    const uint16_t min_samples,
                                    const uint16_t max_samples )
{
  if ( session_.has_value() ) {
    session_->cursor.set_target_lag( target_samples, min_samples, max_samples );
  }
}
