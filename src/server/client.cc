#include "client.hh"

using namespace std;

uint64_t Client::client_mix_cursor() const
{
  return mix_cursor_;
}

uint64_t Client::server_mix_cursor() const
{
  return mix_cursor_ + outbound_frame_offset_.value() * opus_frame::NUM_SAMPLES;
}

Client::Client( const uint8_t node_id, const uint16_t server_port )
  : Client( node_id, server_port, {}, {} )
{}

Client::Client( const uint8_t node_id,
                const uint16_t server_port,
                const Base64Key& send_key,
                const Base64Key& receive_key )
  : connection( 0, node_id, send_key, receive_key )
  , clock( 0 )
  , cursor( 600, true )
  , encoder( 128000, 48000 )
{
  cerr << "Client " << int( node_id ) << " " << server_port << " " << receive_key.printable_key().as_string_view()
       << " " << send_key.printable_key().as_string_view() << endl;

  /* XXX set default gains */
  for ( uint8_t channel_i = 0; channel_i < 2 * MAX_CLIENTS; channel_i++ ) {
    gains.at( channel_i ) = { 1.0, 1.0 };
  }
}

void Client::receive_packet( const Address& source, const Ciphertext& ciphertext, const uint64_t clock_sample )
{
  if ( connection.receive_packet( source, ciphertext ) ) {
    clock.new_sample( clock_sample, connection.unreceived_beyond_this_frame_index() * opus_frame::NUM_SAMPLES );
    if ( ( not outbound_frame_offset_.has_value() ) and connection.has_destination() ) {
      outbound_frame_offset_ = clock_sample / opus_frame::NUM_SAMPLES;
    }
  }
}

void Client::decode_audio( const uint64_t clock_sample, const uint64_t cursor_sample )
{
  clock.time_passes( clock_sample );

  cursor.sample( connection.frames(), cursor_sample, clock.value(), clock.jitter(), decoded_audio );

  connection.pop_frames( min( cursor.ok_to_pop( connection.frames() ),
                              connection.next_frame_needed() - connection.frames().range_begin() ) );
}

void Client::mix_and_encode( const vector<Client>& clients, const uint64_t cursor_sample )
{
  if ( not outbound_frame_offset_.has_value() ) {
    return;
  }

  while ( server_mix_cursor() + opus_frame::NUM_SAMPLES <= cursor_sample ) {
    span<float> ch1 = mixed_audio.ch1().region( client_mix_cursor(), opus_frame::NUM_SAMPLES );
    span<float> ch2 = mixed_audio.ch2().region( client_mix_cursor(), opus_frame::NUM_SAMPLES );

    // XXX need to scale gains with newly arriving clients
    for ( uint8_t channel_i = 0; channel_i < 2 * clients.size(); channel_i += 2 ) {
      const Client& other_client = clients.at( channel_i / 2 );
      const span_view<float> other_ch1
        = other_client.decoded_audio.ch1().region( server_mix_cursor(), opus_frame::NUM_SAMPLES );
      const span_view<float> other_ch2
        = other_client.decoded_audio.ch2().region( server_mix_cursor(), opus_frame::NUM_SAMPLES );

      const float gain1into1 = gains[channel_i].first;
      const float gain1into2 = gains[channel_i].second;
      const float gain2into1 = gains[channel_i + 1].first;
      const float gain2into2 = gains[channel_i + 1].second;

      for ( size_t sample_i = 0; sample_i < opus_frame::NUM_SAMPLES; sample_i++ ) {
        ch1[sample_i] += gain1into1 * other_ch1[sample_i] + gain2into1 * other_ch2[sample_i];
        ch2[sample_i] += gain1into2 * other_ch1[sample_i] + gain2into2 * other_ch2[sample_i];
      }
    }

    mix_cursor_ += opus_frame::NUM_SAMPLES;
  }

  /* encode audio */
  while ( encoder.min_encode_cursor() + opus_frame::NUM_SAMPLES <= client_mix_cursor() ) {
    encoder.encode_one_frame( mixed_audio.ch1(), mixed_audio.ch2() );
    connection.push_frame( encoder );
  }

  /* pop used mixed audio */
  mixed_audio.pop( encoder.min_encode_cursor() - mixed_audio.range_begin() );
}

void Client::pop_decoded_audio( const uint64_t cursor_sample )
{
  if ( outbound_frame_offset_.has_value() ) {
    decoded_audio.pop( server_mix_cursor() - decoded_audio.range_begin() );
  } else {
    decoded_audio.pop( ( cursor_sample / opus_frame::NUM_SAMPLES ) * opus_frame::NUM_SAMPLES
                       - decoded_audio.range_begin() );
  }
}

void Client::send_packet( UDPSocket& socket )
{
  if ( connection.has_destination() ) {
    connection.send_packet( socket );
  }
}

void Client::summary( ostream& out ) const
{
  if ( connection.has_destination() ) {
    out << " (" << connection.destination().to_string() << ") ";
  }
  clock.summary( out );
  cursor.summary( out );
  connection.summary( out );
}
