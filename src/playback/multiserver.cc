#include "multiserver.hh"

#include <chrono>
#include <iostream>

using namespace std;
using namespace std::chrono;

NetworkMultiServer::NetworkMultiServer( EventLoop& loop )
  : socket_()
  , next_cursor_sample( Timer::timestamp_ns() + cursor_sample_interval )
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
      next_cursor_sample = now + cursor_sample_interval;
      for ( Client& cl : clients_ ) {
        cl.cursor.sample( *cl.endpoint, now );
      }
    },
    [&] { return Timer::timestamp_ns() >= next_cursor_sample; } );
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

  /* find matching client */
  for ( Client& client : clients_ ) {
    if ( client.addr == src ) {
      client.receive_packet( plaintext );

      /* XXX send an ACK */
      client.endpoint->send_packet( crypto_, client.addr, socket_ );
      return;
    }
  }

  /* no matching client? */
  clients_.emplace_back( src );
  Client& client = clients_.back();
  client.receive_packet( plaintext );

  /* XXX send an ACK */
  client.endpoint->send_packet( crypto_, client.addr, socket_ );
}

NetworkMultiServer::Client::Client( const Address& s_addr )
  : addr( s_addr )
  , cursor( 1, true )
{}

void NetworkMultiServer::Client::receive_packet( Plaintext& plaintext )
{
  endpoint->receive_packet( plaintext );

  /* throw away frames for now */
  if ( cursor.next_frame_index() > endpoint->frames().range_begin() ) {
    endpoint->pop_frames( cursor.next_frame_index() - endpoint->frames().range_begin() );
  }
}

void NetworkMultiServer::summary( ostream& out ) const
{
  out << "Network multiserver:";

  if ( stats_.decryption_failures ) {
    out << " decryption_failures=" << stats_.decryption_failures;
  }

  out << "\n";

  for ( const auto& client : clients_ ) {
    client.summary( out );
  }
}

void NetworkMultiServer::Client::summary( ostream& out ) const
{
  out << "   " << addr.to_string() << " (" << endpoint->next_frame_needed() << "):";
  cursor.summary( out );
  out << "\n";
  endpoint->summary( out );
}
