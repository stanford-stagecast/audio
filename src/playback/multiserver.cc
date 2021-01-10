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

void NetworkMultiServer::Client::receive_packet( const Address& source,
                                                 const Ciphertext& ciphertext,
                                                 const uint64_t clock_sample )
{
  if ( connection.receive_packet( source, ciphertext ) ) {
    clock.new_sample( clock_sample, connection.unreceived_beyond_this_frame_index() * opus_frame::NUM_SAMPLES );
    if ( ( not outbound_frame_offset_.has_value() ) and connection.has_destination() ) {
      outbound_frame_offset_ = clock_sample / opus_frame::NUM_SAMPLES;
    }
  }
}

void NetworkMultiServer::Client::decode_audio( const uint64_t clock_sample, const uint64_t cursor_sample )
{
  clock.time_passes( clock_sample );

  cursor.sample( connection.frames(), cursor_sample, clock.value(), clock.jitter(), decoded_audio );

  connection.pop_frames( min( cursor.ok_to_pop( connection.frames() ),
                              connection.next_frame_needed() - connection.frames().range_begin() ) );
}

void NetworkMultiServer::Client::mix_and_encode( const std::array<std::optional<Client>, MAX_CLIENTS>& clients,
                                                 const uint64_t cursor_sample )
{
  if ( not outbound_frame_offset_.has_value() ) {
    return;
  }

  while ( server_mix_cursor() + opus_frame::NUM_SAMPLES <= cursor_sample ) {
    span<float> ch1 = mixed_audio.ch1().region( client_mix_cursor(), opus_frame::NUM_SAMPLES );
    span<float> ch2 = mixed_audio.ch2().region( client_mix_cursor(), opus_frame::NUM_SAMPLES );

    for ( uint8_t channel_i = 0; channel_i < 2 * MAX_CLIENTS; channel_i += 2 ) {
      const Client& other_client = clients.at( channel_i / 2 ).value();
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

void NetworkMultiServer::Client::pop_decoded_audio( const uint64_t cursor_sample )
{
  if ( outbound_frame_offset_.has_value() ) {
    decoded_audio.pop( server_mix_cursor() - decoded_audio.range_begin() );
  } else {
    decoded_audio.pop( ( cursor_sample / opus_frame::NUM_SAMPLES ) * opus_frame::NUM_SAMPLES
                       - decoded_audio.range_begin() );
  }
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

  loop.add_rule( "network receive", socket_, Direction::In, [&] {
    Address src { nullptr, 0 };
    Ciphertext ciphertext;
    socket_.recv( src, ciphertext.data );
    if ( ciphertext.size() > 20 ) {
      const uint8_t node_id = ciphertext.data.back();
      if ( node_id > 0 and node_id <= MAX_CLIENTS ) {
        auto& client = clients_.at( node_id - 1 ).value();
        client.receive_packet( src, ciphertext, server_clock() );
      }
    }
  } );

  loop.add_rule(
    "mix+encode+send",
    [&] {
      const auto clock_sample = server_clock();

      /* decode all audio */
      for ( auto& client_opt : clients_ ) {
        client_opt.value().decode_audio( clock_sample, next_cursor_sample_ );
      }

      /* mix all audio */
      for ( auto& client_opt : clients_ ) {
        client_opt.value().mix_and_encode( clients_, next_cursor_sample_ );
      }

      /* send audio to clients */
      for ( auto& client_opt : clients_ ) {
        client_opt.value().send_packet( socket_ );
      }

      /* pop used decoded audio */
      for ( auto& client_opt : clients_ ) {
        client_opt.value().pop_decoded_audio( next_cursor_sample_ );
      }

      next_cursor_sample_ += opus_frame::NUM_SAMPLES;
    },
    [&] { return server_clock() >= next_cursor_sample_; } );
}

void NetworkMultiServer::Client::send_packet( UDPSocket& socket )
{
  if ( connection.has_destination() ) {
    connection.send_packet( socket );
  }
}

void NetworkMultiServer::summary( ostream& out ) const
{
  for ( unsigned int i = 0; i < clients_.size(); i++ ) {
    auto& client = clients_.at( i ).value();
    out << "#" << i + 1 << ": ";
    client.summary( out );
  }
}

void NetworkMultiServer::Client::summary( ostream& out ) const
{
  if ( connection.has_destination() ) {
    out << " (" << connection.destination().to_string() << ") ";
  }
  clock.summary( out );
  cursor.summary( out );
  connection.summary( out );
}
