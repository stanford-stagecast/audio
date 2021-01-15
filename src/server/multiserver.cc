#include "multiserver.hh"

#include <chrono>
#include <iostream>

using namespace std;
using namespace std::chrono;

uint64_t NetworkMultiServer::server_clock() const
{
  return ( Timer::timestamp_ns() - global_ns_timestamp_at_creation_ ) * 48 / 1000000;
}

void NetworkMultiServer::add_client()
{
  const size_t index = clients_.get_index();
  if ( index >= numeric_limits<uint8_t>::max() ) {
    throw runtime_error( "too many clients" );
  }
  clients_.insert( index, Client( index + 1, socket_.local_address().port() ) );
}

void NetworkMultiServer::add_key( const LongLivedKey& key )
{
  keys_.push_back( key );
  cerr << "Added key for: " << key.name() << "\n";
}

NetworkMultiServer::NetworkMultiServer( EventLoop& loop )
  : socket_()
  , global_ns_timestamp_at_creation_( Timer::timestamp_ns() )
  , next_cursor_sample_( server_clock() + opus_frame::NUM_SAMPLES )
{
  socket_.set_blocking( false );
  socket_.bind( { "0", 0 } );

  /* XXX create some clients */
  add_client();
  add_client();
  add_client();
  add_client();

  loop.add_rule( "network receive", socket_, Direction::In, [&] {
    Address src { nullptr, 0 };
    Ciphertext ciphertext;
    socket_.recv( src, ciphertext.data );
    if ( ciphertext.size() > 20 ) {
      const uint8_t node_id = ciphertext.data.back();
      if ( node_id > 0 and clients_.has_value( node_id - 1 ) ) {
        clients_.at( node_id - 1 ).receive_packet( src, ciphertext, server_clock() );
      }
    } else {
      stats_.bad_packets++;
    }
  } );

  loop.add_rule(
    "mix+encode+send",
    [&] {
      const auto clock_sample = server_clock();

      /* decode all audio */
      for ( auto& client : clients_ ) {
        client.decode_audio( clock_sample, next_cursor_sample_ );
      }

      /* mix all audio */
      for ( auto& client : clients_ ) {
        client.mix_and_encode( clients_, next_cursor_sample_ );
      }

      /* send audio to clients */
      for ( auto& client : clients_ ) {
        client.send_packet( socket_ );
      }

      /* pop used decoded audio */
      for ( auto& client : clients_ ) {
        client.pop_decoded_audio( next_cursor_sample_ );
      }

      next_cursor_sample_ += opus_frame::NUM_SAMPLES;
    },
    [&] { return server_clock() >= next_cursor_sample_; } );
}

void NetworkMultiServer::summary( ostream& out ) const
{
  out << "bad packets: " << stats_.bad_packets << "\n";
  for ( const auto& client : clients_ ) {
    out << "#" << int( client.peer_id() ) << ": ";
    client.summary( out );
  }
}
