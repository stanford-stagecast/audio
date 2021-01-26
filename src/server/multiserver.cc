#include "multiserver.hh"

#include <chrono>
#include <iostream>

using namespace std;
using namespace std::chrono;

uint64_t NetworkMultiServer::server_clock() const
{
  return ( Timer::timestamp_ns() - global_ns_timestamp_at_creation_ ) * 48 / 1000000;
}

void NetworkMultiServer::receive_keyrequest( const Address& src, const Ciphertext& ciphertext )
{
  /* decrypt */
  for ( auto& client : clients_ ) {
    if ( client.try_keyrequest( src, ciphertext, socket_ ) ) {
      return;
    }
  }
  stats_.bad_packets++;
}

void NetworkMultiServer::add_key( const LongLivedKey& key )
{
  const uint8_t next_id = clients_.size() + 1;
  clients_.emplace_back( next_id, key );
  cerr << "Added key #" << int( next_id ) << " for: " << key.name() << "\n";
}

NetworkMultiServer::NetworkMultiServer( EventLoop& loop )
  : socket_()
  , global_ns_timestamp_at_creation_( Timer::timestamp_ns() )
  , next_cursor_sample_( server_clock() + opus_frame::NUM_SAMPLES )
{
  socket_.set_blocking( false );
  socket_.bind( { "0", 9101 } );

  loop.add_rule( "network receive", socket_, Direction::In, [&] {
    Address src { nullptr, 0 };
    Ciphertext ciphertext;
    ciphertext.resize( socket_.recv( src, ciphertext.mutable_buffer() ) );
    if ( ciphertext.length() > 24 ) {
      const uint8_t node_id = ciphertext.as_string_view().back();
      if ( node_id == uint8_t( KeyMessage::keyreq_id ) ) {
        receive_keyrequest( src, ciphertext );
      } else if ( node_id > 0 and node_id <= clients_.size() ) {
        clients_.at( node_id - 1 ).receive_packet( src, ciphertext, server_clock() );
      } else {
        stats_.bad_packets++;
      }
    } else {
      stats_.bad_packets++;
    }
  } );

  loop.add_rule(
    "mix+encode+send",
    [&] {
      const uint64_t ts_now = Timer::timestamp_ns();
      const auto clock_sample = server_clock();

      /* decode all audio */
      for ( auto& client : clients_ ) {
        if ( client ) {
          client.client().decode_audio( clock_sample, next_cursor_sample_ );
          if ( client.client().connection().sender_stats().last_good_ack_ts + CLIENT_TIMEOUT_NS < ts_now ) {
            client.clear_current_session();
          }
        }
      }

      /* mix all audio */
      for ( auto& client : clients_ ) {
        if ( client ) {
          client.client().mix_and_encode( clients_, next_cursor_sample_ );
        }
      }

      /* send audio to clients */
      for ( auto& client : clients_ ) {
        if ( client ) {
          client.client().send_packet( socket_ );
          client.client().pop_decoded_audio( next_cursor_sample_ );
        }
      }

      next_cursor_sample_ += opus_frame::NUM_SAMPLES;
    },
    [&] { return server_clock() >= next_cursor_sample_; } );
}

void NetworkMultiServer::summary( ostream& out ) const
{
  out << "bad packets: " << stats_.bad_packets << "\n";
  for ( const auto& client : clients_ ) {
    if ( client ) {
      out << "#" << int( client.client().peer_id() ) << ": ";
      client.summary( out );
    }
  }
}
