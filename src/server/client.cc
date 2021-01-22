#include "client.hh"

using namespace std;
using namespace chrono;

uint64_t Client::client_mix_cursor() const
{
  return mix_cursor_;
}

uint64_t Client::server_mix_cursor() const
{
  return mix_cursor_ + outbound_frame_offset_.value() * opus_frame::NUM_SAMPLES;
}

Client::Client( const uint8_t node_id, CryptoSession&& crypto )
  : connection_( 0, node_id, move( crypto ) )
  , clock_( 0 )
  , cursor_( 900, false )
  , encoder_( 64000, 48000 )
{
  /* XXX set default gains */
  for ( uint8_t channel_i = 0; channel_i < 2 * MAX_CLIENTS; channel_i++ ) {
    gains_.at( channel_i ) = { 2.0, 2.0 };
    if ( channel_i / 2 == node_id - 1 ) {
      gains_.at( channel_i ) = { 0.0, 0.0 };
    }
  }
}

bool Client::receive_packet( const Address& source, const Ciphertext& ciphertext, const uint64_t clock_sample )
{
  if ( connection_.receive_packet( ciphertext, source ) ) {
    clock_.new_sample( clock_sample, connection_.unreceived_beyond_this_frame_index() * opus_frame::NUM_SAMPLES );
    if ( ( not outbound_frame_offset_.has_value() ) and connection_.has_destination() ) {
      outbound_frame_offset_ = clock_sample / opus_frame::NUM_SAMPLES;
    }
    return true;
  }
  return false;
}

void Client::decode_audio( const uint64_t clock_sample, const uint64_t cursor_sample )
{
  clock_.time_passes( clock_sample );

  cursor_.sample( connection_.frames(), cursor_sample, clock_.value(), clock_.jitter(), decoded_audio_ );

  connection_.pop_frames( min( cursor_.ok_to_pop( connection_.frames() ),
                               connection_.next_frame_needed() - connection_.frames().range_begin() ) );
}

void Client::mix_and_encode( const vector<KnownClient>& clients, const uint64_t cursor_sample )
{
  if ( not outbound_frame_offset_.has_value() ) {
    return;
  }

  while ( server_mix_cursor() + opus_frame::NUM_SAMPLES <= cursor_sample ) {
    span<float> ch1 = mixed_audio_.ch1().region( client_mix_cursor(), opus_frame::NUM_SAMPLES );
    span<float> ch2 = mixed_audio_.ch2().region( client_mix_cursor(), opus_frame::NUM_SAMPLES );

    // XXX need to scale gains with newly arriving clients
    for ( uint8_t channel_i = 0; channel_i < min( size_t( 2 * MAX_CLIENTS ), clients.size() * 2 );
          channel_i += 2 ) {
      if ( clients.at( channel_i / 2 ) ) {
        const Client& other_client = clients.at( channel_i / 2 ).client();
        /*
        if ( other_client.decoded_audio_.range_begin() > server_mix_cursor()
             or other_client.decoded_audio_.range_end() < server_mix_cursor() + opus_frame::NUM_SAMPLES ) {
          continue;
        }
        */
        const span_view<float> other_ch1
          = other_client.decoded_audio_.ch1().region( server_mix_cursor(), opus_frame::NUM_SAMPLES );
        const span_view<float> other_ch2
          = other_client.decoded_audio_.ch2().region( server_mix_cursor(), opus_frame::NUM_SAMPLES );

        const float gain1into1 = gains_[channel_i].first;
        const float gain1into2 = gains_[channel_i].second;
        const float gain2into1 = gains_[channel_i + 1].first;
        const float gain2into2 = gains_[channel_i + 1].second;

        for ( size_t sample_i = 0; sample_i < opus_frame::NUM_SAMPLES; sample_i++ ) {
          ch1[sample_i] += gain1into1 * other_ch1[sample_i] + gain2into1 * other_ch2[sample_i];
          ch2[sample_i] += gain1into2 * other_ch1[sample_i] + gain2into2 * other_ch2[sample_i];
        }
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

void Client::pop_decoded_audio( const uint64_t cursor_sample )
{
  if ( outbound_frame_offset_.has_value() ) {
    decoded_audio_.pop_before( server_mix_cursor() );
  } else {
    decoded_audio_.pop_before( ( cursor_sample / opus_frame::NUM_SAMPLES ) * opus_frame::NUM_SAMPLES );
  }
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
  clock_.summary( out );
  cursor_.summary( out );
  connection_.summary( out );
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
       and ( plaintext.size() == 0 ) ) {
    stats_.key_requests++;
    if ( steady_clock::now() < next_reply_allowed_ ) {
      return true;
    }

    /* reply with keys to next session */
    Plaintext outgoing_keys;
    {
      Serializer s { outgoing_keys };
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

KnownClient::KnownClient( const uint8_t node_id, const LongLivedKey& key )
  : id_( node_id )
  , name_( key.name() )
  , long_lived_crypto_( key.key_pair().downlink, key.key_pair().uplink, true )
  , next_reply_allowed_( steady_clock::now() )
  , next_session_( CryptoSession { next_keys_.downlink, next_keys_.uplink } )
{}

void KnownClient::receive_packet( const Address& src, const Ciphertext& ciphertext, const uint64_t clock_sample )
{
  if ( current_session_.has_value() and current_session_->receive_packet( src, ciphertext, clock_sample ) ) {
    return;
  }

  Plaintext throwaway_plaintext;
  if ( next_session_.value().decrypt( ciphertext, { &id_, 1 }, throwaway_plaintext ) ) {
    /* new session established */
    current_session_.emplace( id_, move( next_session_.value() ) );
    current_session_->pop_decoded_audio( clock_sample );

    next_keys_ = KeyPair {};
    next_session_.emplace( next_keys_.downlink, next_keys_.uplink );
    stats_.new_sessions++;

    /* actually use packet */
    current_session_->receive_packet( src, ciphertext, clock_sample );
  }
}
