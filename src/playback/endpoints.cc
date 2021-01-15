#include "endpoints.hh"
#include "timestamp.hh"

using namespace std;

NetworkClient::NetworkClient( const uint8_t node_id,
                              const Address& server,
                              const Base64Key& send_key,
                              const Base64Key& receive_key,
                              shared_ptr<OpusEncoderProcess> source,
                              shared_ptr<AudioDeviceTask> dest,
                              EventLoop& loop )
  : NetworkConnection( node_id, 0, send_key, receive_key, server )
  , socket_()
  , source_( source )
  , dest_( dest )
  , peer_clock_( dest_->cursor() )
  , cursor_( 720, false )
  , next_cursor_sample_( dest_->cursor() + opus_frame::NUM_SAMPLES )
{
  socket_.set_blocking( false );

  loop.add_rule(
    "network transmit",
    [&] {
      push_frame( *source_ );
      send_packet( socket_ );
    },
    [&] { return source_->has_frame(); } );

  loop.add_rule( "network receive", socket_, Direction::In, [&] {
    Address src { nullptr, 0 };
    Ciphertext ciphertext;
    socket_.recv( src, ciphertext.data );
    receive_packet( src, ciphertext );
    peer_clock_.new_sample( dest_->cursor(), unreceived_beyond_this_frame_index() * opus_frame::NUM_SAMPLES );
  } );

  loop.add_rule(
    "decode",
    [&] {
      peer_clock_.time_passes( dest_->cursor() );

      /* decode server's Opus frames to playback buffer */
      cursor_.sample( frames(), next_cursor_sample_, peer_clock_.value(), peer_clock_.jitter(), dest_->playback() );

      /* pop used Opus frames from server */
      pop_frames( min( cursor_.ok_to_pop( frames() ), next_frame_needed() - frames().range_begin() ) );

      next_cursor_sample_ += opus_frame::NUM_SAMPLES;
    },
    [&] { return dest_->cursor() + opus_frame::NUM_SAMPLES + 60 >= next_cursor_sample_; } );
}

uint64_t NetworkSingleServer::client_mix_cursor() const
{
  return mix_cursor_;
}

uint64_t NetworkSingleServer::server_mix_cursor() const
{
  return mix_cursor_ + outbound_frame_offset_.value() * opus_frame::NUM_SAMPLES;
}

NetworkSingleServer::NetworkSingleServer( EventLoop& loop, const Base64Key& send_key, const Base64Key& receive_key )
  : NetworkConnection( 0, 1, send_key, receive_key )
  , socket_()
  , global_ns_timestamp_at_creation_( Timer::timestamp_ns() )
  , next_cursor_sample_( server_clock() + opus_frame::NUM_SAMPLES )
  , peer_clock_( server_clock() )
  , cursor_( 720, false )
  , encoder_( 128000, 48000 )
{
  socket_.set_blocking( false );
  socket_.bind( { "0", 0 } );
  cerr << "Port " << socket_.local_address().port() << " " << receive_key.printable_key().as_string_view() << " "
       << send_key.printable_key().as_string_view() << endl;

  loop.add_rule( "network receive", socket_, Direction::In, [&] {
    Address src { nullptr, 0 };
    Ciphertext ciphertext;
    socket_.recv( src, ciphertext.data );
    receive_packet( src, ciphertext );

    const auto clock_sample = server_clock();
    peer_clock_.new_sample( clock_sample, unreceived_beyond_this_frame_index() * opus_frame::NUM_SAMPLES );
    if ( ( not outbound_frame_offset_.has_value() ) and has_destination() ) {
      outbound_frame_offset_ = clock_sample / opus_frame::NUM_SAMPLES;
    }
  } );

  loop.add_rule(
    "mix+encode+send",
    [&] {
      peer_clock_.time_passes( server_clock() );

      /* decode client's Opus frames to output buffer */
      cursor_.sample( frames(), next_cursor_sample_, peer_clock_.value(), peer_clock_.jitter(), decoded_audio_ );

      /* pop used Opus frames from client */
      pop_frames( min( cursor_.ok_to_pop( frames() ), next_frame_needed() - frames().range_begin() ) );

      if ( outbound_frame_offset_.has_value() ) {
        /* mix audio */
        while ( server_mix_cursor() + opus_frame::NUM_SAMPLES <= next_cursor_sample_ ) {
          span<float> ch1 = mixed_audio_.ch1().region( client_mix_cursor(), opus_frame::NUM_SAMPLES );
          span<float> ch2 = mixed_audio_.ch2().region( client_mix_cursor(), opus_frame::NUM_SAMPLES );

          fill( ch1.begin(), ch1.end(), 0.0 ); /* leave left channel silent */

          /* copy left channel to right */
          for ( unsigned int i = 0; i < opus_frame::NUM_SAMPLES; i++ ) {
            ch2.at( i ) = decoded_audio_.ch1().at( server_mix_cursor() + i );
          }

          mix_cursor_ += opus_frame::NUM_SAMPLES;
        }

        /* pop used decoded audio */
        decoded_audio_.pop_before( server_mix_cursor() );

        /* encode audio */
        while ( encoder_.min_encode_cursor() + opus_frame::NUM_SAMPLES <= client_mix_cursor() ) {
          /* encode */
          encoder_.encode_one_frame( mixed_audio_.ch1(), mixed_audio_.ch2() );

          /* transmit */
          push_frame( encoder_ );
          send_packet( socket_ );
        }

        /* pop used mixed audio */
        mixed_audio_.pop_before( encoder_.min_encode_cursor() );
      } else {
        /* pop ignored decoded audio */
        decoded_audio_.pop_before( ( next_cursor_sample_ / opus_frame::NUM_SAMPLES ) * opus_frame::NUM_SAMPLES );
      }

      next_cursor_sample_ += opus_frame::NUM_SAMPLES;
    },
    [&] { return server_clock() >= next_cursor_sample_; } );
}

NetworkSingleServer::NetworkSingleServer( EventLoop& loop )
  : NetworkSingleServer( loop, {}, {} )
{}

uint64_t NetworkSingleServer::server_clock() const
{
  return ( Timer::timestamp_ns() - global_ns_timestamp_at_creation_ ) * 48 / 1000000;
}

void NetworkSingleServer::summary( ostream& out ) const
{
  out << "Server clock: ";
  pp_samples( out, server_clock() );
  out << "\n";
  out << "Peer: ";
  peer_clock_.summary( out );
  cursor_.summary( out );

  NetworkConnection::summary( out );
}

void NetworkClient::summary( ostream& out ) const
{
  out << "Peer: ";
  peer_clock_.summary( out );
  cursor_.summary( out );

  NetworkConnection::summary( out );
}
