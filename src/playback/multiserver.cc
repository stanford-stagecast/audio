#include "multiserver.hh"

#include <iostream>

using namespace std;

NetworkMultiServer::NetworkMultiServer( EventLoop& loop )
  : socket_()
{
  socket_.set_blocking( false );
  socket_.bind( { "0", 0 } );

  cout << "Port " << socket_.local_address().port() << " " << receive_key_.printable_key().as_string_view() << " "
       << send_key_.printable_key().as_string_view() << endl;

  loop.add_rule( "network receive", socket_, Direction::In, [&] { receive_packet(); } );
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
      service_client( client, plaintext );
      return;
    }
  }

  /* no matching client? */
  clients_.emplace_back( src );
  service_client( clients_.back(), plaintext );
}

void NetworkMultiServer::service_client( Client& client, Plaintext& plaintext )
{
  auto& endpoint = *client.endpoint.get();

  endpoint.receive_packet( plaintext );

  /* throw away frames for now */
  endpoint.pop_frames( endpoint.next_frame_needed() - endpoint.frames().range_begin() );

  /* send an ACK */
  endpoint.send_packet( crypto_, client.addr, socket_ );
}

void NetworkMultiServer::summary( ostream& out ) const
{
  out << "Network multiserver:";

  if ( stats_.decryption_failures ) {
    out << " decryption_failures=" << stats_.decryption_failures;
  }

  out << "\n";

  for ( const auto& client : clients_ ) {
    out << "   " << client.addr.to_string() << ":";
    client.endpoint->summary( out );
  }
}
