#include "multiserver.hh"

#include <chrono>
#include <iostream>

using namespace std;
using namespace std::chrono;

uint64_t NetworkMultiServer::server_clock() const
{
  return ( Timer::timestamp_ns() - global_ns_timestamp_at_creation_ ) * 48 / 1000000;
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

void NetworkMultiServer::summary( ostream& out ) const
{
  for ( unsigned int i = 0; i < clients_.size(); i++ ) {
    auto& client = clients_.at( i ).value();
    out << "#" << i + 1 << ": ";
    client.summary( out );
  }
}
