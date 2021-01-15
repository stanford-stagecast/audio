#include "multiserver.hh"

#include <chrono>
#include <iostream>

using namespace std;
using namespace std::chrono;

uint64_t NetworkMultiServer::server_clock() const
{
  return ( Timer::timestamp_ns() - global_ns_timestamp_at_creation_ ) * 48 / 1000000;
}

uint64_t NetworkMultiServer::Client::client_mix_cursor() const
{
  return mix_cursor_;
}

uint64_t NetworkMultiServer::Client::server_mix_cursor() const
{
  return mix_cursor_ + outbound_frame_offset_.value() * opus_frame::NUM_SAMPLES;
}

NetworkMultiServer::Client::Client( const uint8_t node_id, const uint16_t server_port )
  : Client( node_id, server_port, {}, {} )
{}

NetworkMultiServer::Client::Client( const uint8_t node_id,
                                    const uint16_t server_port,
                                    const Base64Key& send_key,
                                    const Base64Key& receive_key )
  : connection( 0, node_id, send_key, receive_key )
  , clock( 0 )
  , cursor( 960, false )
  , encoder( 128000, 48000 )
{
  cerr << "Client " << int( node_id ) << " " << server_port << " " << receive_key.printable_key().as_string_view()
       << " " << send_key.printable_key().as_string_view() << endl;
}

NetworkMultiServer::NetworkMultiServer( EventLoop& loop )
  : socket_()
  , global_ns_timestamp_at_creation_( Timer::timestamp_ns() )
  , next_cursor_sample_( server_clock() + opus_frame::NUM_SAMPLES )
{
  socket_.set_blocking( false );
  socket_.bind( { "0", 0 } );

  /* construct clients */
  {
    const uint16_t server_port = socket_.local_address().port();
    for ( unsigned int i = 0; i < MAX_CLIENTS; i++ ) {
      clients_.at( i ).emplace( i + 1, server_port );
    }
  }

  /* set up mixing constants XXX */
  for ( unsigned int i = 0; i < MAX_CLIENTS; i++ ) {
    auto& client = clients_.at( i ).value();
    for ( uint8_t channel_i = 0; channel_i < 2 * MAX_CLIENTS; channel_i++ ) {
      if ( channel_i / 2 == i ) {
        client.gains[channel_i] = { 0.0, 0.0 };
      } else {
        client.gains[channel_i] = { 2.0, 2.0 };
      }
    }
  }

  loop.add_rule( "network receive", socket_, Direction::In, [&] {
    Address src { nullptr, 0 };
    Ciphertext ciphertext;
    socket_.recv( src, ciphertext.data );
    if ( ciphertext.size() > 20 ) {
      const uint8_t node_id = ciphertext.data.back();
      if ( node_id > 0 and node_id <= MAX_CLIENTS ) {
        auto& client = clients_.at( node_id - 1 ).value();
        if ( client.connection.receive_packet( src, ciphertext ) ) {
          const auto clock_sample = server_clock();
          client.clock.new_sample(
            clock_sample, client.connection.unreceived_beyond_this_frame_index() * opus_frame::NUM_SAMPLES );
          if ( ( not client.outbound_frame_offset_.has_value() ) and client.connection.has_destination() ) {
            client.outbound_frame_offset_ = clock_sample / opus_frame::NUM_SAMPLES;
          }
        }
      }
    }
  } );

  loop.add_rule(
    "mix+encode+send",
    [&] {
      const auto clock_sample = server_clock();

      /* decode all audio */
      for ( auto& client_opt : clients_ ) {
        auto& client = client_opt.value();
        client.clock.time_passes( clock_sample );

        client.cursor.sample( client.connection.frames(),
                              next_cursor_sample_,
                              client.clock.value(),
                              client.clock.jitter(),
                              client.decoded_audio );

        client.connection.pop_frames(
          min( client.cursor.ok_to_pop( client.connection.frames() ),
               client.connection.next_frame_needed() - client.connection.frames().range_begin() ) );
      }

      /* mix all audio */
      for ( auto& client_opt : clients_ ) {
        auto& client = client_opt.value();

        if ( not client.outbound_frame_offset_.has_value() ) {
          continue;
        }

        while ( client.server_mix_cursor() + opus_frame::NUM_SAMPLES <= next_cursor_sample_ ) {
          span<float> ch1 = client.mixed_audio.ch1().region( client.client_mix_cursor(), opus_frame::NUM_SAMPLES );
          span<float> ch2 = client.mixed_audio.ch2().region( client.client_mix_cursor(), opus_frame::NUM_SAMPLES );

          for ( uint8_t channel_i = 0; channel_i < 2 * MAX_CLIENTS; channel_i += 2 ) {
            const Client& other_client = clients_.at( channel_i / 2 ).value();
            const span_view<float> other_ch1
              = other_client.decoded_audio.ch1().region( client.server_mix_cursor(), opus_frame::NUM_SAMPLES );
            const span_view<float> other_ch2
              = other_client.decoded_audio.ch2().region( client.server_mix_cursor(), opus_frame::NUM_SAMPLES );

            const float gain1into1 = client.gains[channel_i].first;
            const float gain1into2 = client.gains[channel_i].second;
            const float gain2into1 = client.gains[channel_i + 1].first;
            const float gain2into2 = client.gains[channel_i + 1].second;

            for ( size_t sample_i = 0; sample_i < opus_frame::NUM_SAMPLES; sample_i++ ) {
              ch1[sample_i] += gain1into1 * other_ch1[sample_i];
              ch2[sample_i] += gain1into2 * other_ch1[sample_i];

              ch1[sample_i] += gain2into1 * other_ch2[sample_i];
              ch2[sample_i] += gain2into2 * other_ch2[sample_i];
            }
          }

          client.mix_cursor_ += opus_frame::NUM_SAMPLES;
        }

        /* encode audio */
        while ( client.encoder.min_encode_cursor() + opus_frame::NUM_SAMPLES <= client.client_mix_cursor() ) {
          client.encoder.encode_one_frame( client.mixed_audio.ch1(), client.mixed_audio.ch2() );
          client.connection.push_frame( client.encoder );
          client.connection.send_packet( socket_ );
        }

        /* pop used mixed audio */
        client.mixed_audio.pop( client.encoder.min_encode_cursor() - client.mixed_audio.range_begin() );
      }

      /* pop used decoded audio */
      for ( auto& client_opt : clients_ ) {
        auto& client = client_opt.value();

        if ( client.outbound_frame_offset_.has_value() ) {
          client.decoded_audio.pop( client.server_mix_cursor() - client.decoded_audio.range_begin() );
        } else {
          client.decoded_audio.pop( ( next_cursor_sample_ / opus_frame::NUM_SAMPLES ) * opus_frame::NUM_SAMPLES
                                    - client.decoded_audio.range_begin() );
        }
      }

      next_cursor_sample_ += opus_frame::NUM_SAMPLES;
    },
    [&] { return server_clock() >= next_cursor_sample_; } );
}

void NetworkMultiServer::summary( ostream& out ) const
{
  for ( unsigned int i = 0; i < clients_.size(); i++ ) {
    auto& client = clients_.at( i ).value();
    out << "#" << i + 1 << ": ";
    if ( client.connection.has_destination() ) {
      out << " (" << client.connection.destination().to_string() << ") ";
    }
    client.clock.summary( out );
    client.cursor.summary( out );
    client.connection.summary( out );
  }
}
