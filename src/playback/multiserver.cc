#include "multiserver.hh"

#include <chrono>
#include <iostream>

using namespace std;
using namespace std::chrono;

NetworkMultiServer::NetworkMultiServer( EventLoop& loop )
  : socket_()
  , next_cursor_sample_( Timer::timestamp_ns() + cursor_sample_interval )
{
  socket_.set_blocking( false );
  socket_.bind( { "0", 0 } );

  cout << "Port " << socket_.local_address().port() << " " << receive_key_.printable_key().as_string_view() << " "
       << send_key_.printable_key().as_string_view() << endl;

  loop.add_rule( "network receive", socket_, Direction::In, [&] { receive_packet(); } );

  loop.add_rule(
    "sample cursors",
    [&] {
      const uint64_t now = Timer::timestamp_ns();
      next_cursor_sample_ = now + cursor_sample_interval;

      /* get mean sample index */
      uint64_t sample_index_sum {}, sample_index_num {};
      for ( auto& cl : clients_ ) {
        if ( cl.has_value() and cl.value().cursor.sample_index().has_value() ) {
          sample_index_num++;
          sample_index_sum += cl.value().cursor.sample_index().value();
        }
      }

      if ( sample_index_num ) {
        global_sample_index_ = sample_index_sum / sample_index_num;
      }

      /* sample each client */
      for ( auto& cl : clients_ ) {
        if ( cl.has_value() ) {
          cl->cursor.sample( cl->endpoint, now, global_sample_index_ );
        }
      }

      /* XXX discard decoded audio */
      for ( auto& cl : clients_ ) {
        if ( cl.has_value() ) {
          cl->cursor.output().pop( global_sample_index_ - cl->cursor.output().ch1().range_begin() );
        }
      }
    },
    [&] { return Timer::timestamp_ns() >= next_cursor_sample_; } );
}

void NetworkMultiServer::receive_packet()
{
  Address src { nullptr, 0 };
  Ciphertext ciphertext;
  socket_.recv( src, ciphertext.data );

  /* decrypt */
  Plaintext plaintext;
  if ( not crypto_.decrypt( ciphertext, plaintext ) ) {
    stats_.decryption_failures++;
    return;
  }

  /* parse */
  Parser parser { plaintext };
  const Packet packet { parser };
  if ( parser.error() ) {
    stats_.invalid++;
    return;
  }

  /* create client? */
  if ( not clients_.at( packet.node_id ).has_value() ) {
    clients_.at( packet.node_id ).emplace( packet.node_id, src );
  }

  Client& client = clients_.at( packet.node_id ).value();

  client.endpoint.act_on_packet( packet );
  /* throw away frames no longer needed */
  if ( client.cursor.next_frame_index() > client.endpoint.frames().range_begin() ) {
    client.endpoint.pop_frames( client.cursor.next_frame_index() - client.endpoint.frames().range_begin() );
  }

  client.endpoint.send_packet( crypto_, client.addr, socket_ ); /* XXX send data */
}

NetworkMultiServer::Client::Client( const uint8_t s_node_id, const Address& s_addr )
  : node_id( s_node_id )
  , addr( s_addr )
  , cursor( 20, true )
{}

void NetworkMultiServer::summary( ostream& out ) const
{
  out << "Network multiserver:";

  if ( stats_.decryption_failures ) {
    out << " decryption_failures=" << stats_.decryption_failures << "!";
  }

  if ( stats_.invalid ) {
    out << " invalid=" << stats_.invalid << "!";
  }

  out << "\n";

  for ( const auto& client : clients_ ) {
    if ( client.has_value() ) {
      client->summary( out );
    }
  }
}

void NetworkMultiServer::Client::summary( ostream& out ) const
{
  out << "   #" << int( node_id ) << "(" << addr.to_string() << "):";
  cursor.summary( out );
  out << "\n";
  endpoint.summary( out );
}
