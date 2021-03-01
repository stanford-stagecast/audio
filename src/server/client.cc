#include "client.hh"

using namespace std;
using namespace chrono;

using Option = RubberBand::RubberBandStretcher::Option;

uint64_t Client::client_mix_cursor() const
{
  return mix_cursor_;
}

uint64_t Client::server_mix_cursor() const
{
  return mix_cursor_ + outbound_frame_offset_.value() * opus_frame::NUM_SAMPLES;
}

AudioFeed::AudioFeed( const string_view name,
                      const uint32_t target_lag_samples,
                      const uint32_t min_lag_samples,
                      const uint32_t max_lag_samples,
                      const bool short_window )
  : name_( name )
  , cursor_( target_lag_samples, min_lag_samples, max_lag_samples )
  , stretcher_( 48000,
                2,
                Option::OptionProcessRealTime | Option::OptionThreadingNever | Option::OptionPitchHighConsistency
                  | ( short_window ? Option::OptionWindowShort : 0 ) )
{
  stretcher_.setMaxProcessSize( opus_frame::NUM_SAMPLES );
  stretcher_.calculateStretch();
}

Client::Client( const uint8_t node_id, const uint8_t ch1_num, const uint8_t ch2_num, CryptoSession&& crypto )
  : connection_( 0, node_id, move( crypto ) )
  , internal_feed_( "internal", 960, 120, 1920, true )
  , quality_feed_( "quality", 4800, 4800 - 240, 4800 + 240, false )
  , ch1_num_( ch1_num )
  , ch2_num_( ch2_num )
{}

bool Client::receive_packet( const Address& source, const Ciphertext& ciphertext, const uint64_t clock_sample )
{
  if ( connection_.receive_packet( ciphertext, source ) ) {
    if ( ( not outbound_frame_offset_.has_value() ) and connection_.has_destination() ) {
      outbound_frame_offset_ = clock_sample / opus_frame::NUM_SAMPLES;
    }

    if ( connection_.has_inbound_unreliable_data() ) {
      Parser p { connection_.inbound_unreliable_data() };
      p.object( last_client_report_ );
      if ( p.error() ) {
        p.clear_error();
      }
      connection_.pop_inbound_unreliable_data();
    }

    return true;
  }
  return false;
}

void AudioFeed::decode_into( const PartialFrameStore<AudioFrame>& frames,
                             const uint64_t cursor_sample,
                             const uint64_t frontier_sample_index,
                             AudioChannel& ch1,
                             AudioChannel& ch2 )
{
  cursor_.setup( cursor_sample, frontier_sample_index );

  Cursor::AudioSlice audio;

  while ( cursor_.initialized() and cursor_sample > cursor_.num_samples_output() ) {
    cursor_.sample( frames, frontier_sample_index, decoder_, stretcher_, audio );

    if ( audio.good ) {
      ch1.region( audio.sample_index, audio.length ).copy( audio.ch1_span() );
      ch2.region( audio.sample_index, audio.length ).copy( audio.ch2_span() );
    }
  }
}

void AudioFeed::decode_into( const PartialFrameStore<AudioFrame>& frames,
                             const uint64_t cursor_sample,
                             const uint64_t frontier_sample_index,
                             AudioChannel& ch1,
                             AudioChannel& ch2,
                             AudioChannel& ch1dup,
                             AudioChannel& ch2dup )
{
  cursor_.setup( cursor_sample, frontier_sample_index );

  Cursor::AudioSlice audio;

  while ( cursor_.initialized() and cursor_sample > cursor_.num_samples_output() ) {
    cursor_.sample( frames, frontier_sample_index, decoder_, stretcher_, audio );

    if ( audio.good ) {
      ch1.region( audio.sample_index, audio.length ).copy( audio.ch1_span() );
      ch2.region( audio.sample_index, audio.length ).copy( audio.ch2_span() );
      ch1dup.region( audio.sample_index, audio.length ).copy( audio.ch1_span() );
      ch2dup.region( audio.sample_index, audio.length ).copy( audio.ch2_span() );
    }
  }
}

void Client::decode_audio( const uint64_t cursor_sample,
                           AudioBoard& internal_board,
                           AudioBoard& quality_board,
                           AudioBoard& quality_board2 )
{
  internal_feed_.decode_into( connection_.frames(),
                              cursor_sample,
                              connection_.unreceived_beyond_this_frame_index() * opus_frame::NUM_SAMPLES,
                              internal_board.channel( ch1_num_ ),
                              internal_board.channel( ch2_num_ ) );

  quality_feed_.decode_into( connection_.frames(),
                             cursor_sample,
                             connection_.unreceived_beyond_this_frame_index() * opus_frame::NUM_SAMPLES,
                             quality_board.channel( ch1_num_ ),
                             quality_board.channel( ch2_num_ ),
                             quality_board2.channel( ch1_num_ ),
                             quality_board2.channel( ch2_num_ ) );

  connection_.pop_frames(
    min( min( internal_feed_.ok_to_pop( connection_.frames() ), quality_feed_.ok_to_pop( connection_.frames() ) ),
         connection_.next_frame_needed() - connection_.frames().range_begin() ) );
}

void Client::mix_and_encode( const AudioBoard& board, const uint64_t cursor_sample )
{
  if ( not outbound_frame_offset_.has_value() ) {
    return;
  }

  while ( server_mix_cursor() + opus_frame::NUM_SAMPLES <= cursor_sample ) {
    span<float> ch1_target = mixed_audio_.ch1().region( client_mix_cursor(), opus_frame::NUM_SAMPLES );
    span<float> ch2_target = mixed_audio_.ch2().region( client_mix_cursor(), opus_frame::NUM_SAMPLES );

    for ( uint8_t channel_i = 0; channel_i < board.num_channels(); channel_i++ ) {
      if ( channel_i == ch1_num_ or channel_i == ch2_num_ ) {
        continue;
      }

      const span_view<float> other_channel
        = board.channel( channel_i ).region( server_mix_cursor(), opus_frame::NUM_SAMPLES );

      const auto [gain_into_1, gain_into_2] = board.gain( channel_i );
      for ( uint8_t sample_i = 0; sample_i < opus_frame::NUM_SAMPLES; sample_i++ ) {
        const float value = other_channel[sample_i];
        const float orig_1 = ch1_target[sample_i];
        const float orig_2 = ch2_target[sample_i];

        ch1_target[sample_i] = orig_1 + gain_into_1 * value;
        ch2_target[sample_i] = orig_2 + gain_into_2 * value;
      }
    }

    mix_cursor_ += opus_frame::NUM_SAMPLES;
  }

  /* encode audio */
  while ( encoder_.min_encode_cursor() + opus_frame::NUM_SAMPLES <= client_mix_cursor() ) {
    encoder_.encode_one_frame( mixed_audio_.ch1(), mixed_audio_.ch2() );
    connection_.push_frame( encoder_ );
  }

  /* pop used mixed audio */
  mixed_audio_.pop_before( encoder_.min_encode_cursor() );
}

void Client::send_packet( UDPSocket& socket )
{
  if ( connection_.has_destination() ) {
    connection_.send_packet( socket );
  }
}

void Client::summary( ostream& out ) const
{
  if ( connection_.has_destination() ) {
    out << " (" << connection_.destination().to_string() << ") ";
  }
  internal_feed_.summary( out );
  quality_feed_.summary( out );
  //  connection_.summary( out );
}

void Client::json_summary( Json::Value& root ) const
{
  internal_feed_.cursor().json_summary( root["feed"][internal_feed_.name()] );
  quality_feed_.cursor().json_summary( root["feed"][quality_feed_.name()] );

  root["client"]["resets"] = last_client_report_.resets;
  root["client"]["target_lag"] = last_client_report_.target_lag;
  root["client"]["min_lag"] = last_client_report_.min_lag;
  root["client"]["max_lag"] = last_client_report_.max_lag;
  root["client"]["actual_lag"] = last_client_report_.actual_lag;
  root["client"]["quality"] = last_client_report_.quality;
  root["client"]["self_gain"] = last_client_report_.self_gain;
}

void Client::default_json_summary( Json::Value& root )
{
  Cursor::default_json_summary( root["feed"]["internal"] );
  Cursor::default_json_summary( root["feed"]["quality"] );

  root["client"]["resets"] = 0;
  root["client"]["target_lag"] = 0;
  root["client"]["min_lag"] = 0;
  root["client"]["max_lag"] = 0;
  root["client"]["actual_lag"] = 0;
  root["client"]["quality"] = 0;
  root["client"]["self_gain"] = 0;
}

void KnownClient::summary( ostream& out ) const
{
  out << name_ << ":";
  out << " requests=" << stats_.key_requests;
  out << " responses=" << stats_.key_responses;
  out << " new_sessions=" << stats_.new_sessions;
  if ( current_session_.has_value() ) {
    current_session_->summary( out );
  }
}

bool KnownClient::try_keyrequest( const Address& src, const Ciphertext& ciphertext, UDPSocket& socket )
{
  Plaintext plaintext;
  if ( long_lived_crypto_.decrypt( ciphertext, { &KeyMessage::keyreq_id, 1 }, plaintext )
       and ( plaintext.length() == 0 ) ) {
    stats_.key_requests++;
    if ( steady_clock::now() < next_reply_allowed_ ) {
      return true;
    }

    /* reply with keys to next session */
    Plaintext outgoing_keys;
    {
      Serializer s { outgoing_keys.mutable_buffer() };
      s.object( KeyMessage { id_, next_keys_ } );
      outgoing_keys.resize( s.bytes_written() );
    }
    Ciphertext outgoing_ciphertext;
    long_lived_crypto_.encrypt( { &KeyMessage::keyreq_server_id, 1 }, outgoing_keys, outgoing_ciphertext );
    socket.sendto( src, outgoing_ciphertext );
    next_reply_allowed_ = steady_clock::now() + milliseconds( 250 );

    stats_.key_responses++;
    return true;
  }
  return false;
}

KnownClient::KnownClient( const uint8_t node_id,
                          const uint8_t ch1_num,
                          const uint8_t ch2_num,
                          const LongLivedKey& key )
  : id_( node_id )
  , name_( key.name() )
  , long_lived_crypto_( key.key_pair().downlink, key.key_pair().uplink, true )
  , next_reply_allowed_( steady_clock::now() )
  , next_session_( CryptoSession { next_keys_.downlink, next_keys_.uplink } )
  , ch1_num_( ch1_num )
  , ch2_num_( ch2_num )
{}

void KnownClient::receive_packet( const Address& src, const Ciphertext& ciphertext, const uint64_t clock_sample )
{
  if ( current_session_.has_value() and current_session_->receive_packet( src, ciphertext, clock_sample ) ) {
    return;
  }

  Plaintext throwaway_plaintext;
  if ( next_session_.value().decrypt( ciphertext, { &id_, 1 }, throwaway_plaintext ) ) {
    /* new session established */
    current_session_.emplace( id_, ch1_num_, ch2_num_, move( next_session_.value() ) );

    next_keys_ = KeyPair {};
    next_session_.emplace( next_keys_.downlink, next_keys_.uplink );
    stats_.new_sessions++;

    /* actually use packet */
    current_session_->receive_packet( src, ciphertext, clock_sample );
  }
}

void Client::set_cursor_lag( const string_view feed,
                             const uint16_t target_samples,
                             const uint16_t min_samples,
                             const uint16_t max_samples )
{
  AudioFeed* target = nullptr;
  if ( internal_feed_.name() == feed ) {
    target = &internal_feed_;
  } else if ( quality_feed_.name() == feed ) {
    target = &quality_feed_;
  }

  if ( target ) {
    target->cursor().set_target_lag( target_samples, min_samples, max_samples );
  }
}
