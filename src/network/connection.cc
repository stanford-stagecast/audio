#include <array>
#include <iostream>

#include "connection.hh"

using namespace std;

NetworkConnection::NetworkConnection( const char node_id,
                                      const char peer_id,
                                      CryptoSession&& crypto,
                                      const Address& destination )
  : NetworkConnection( node_id, peer_id, move( crypto ) )
{
  auto_home_ = false;
  destination_.emplace( destination );
}

NetworkConnection::NetworkConnection( const char node_id, const char peer_id, CryptoSession&& crypto )
  : node_id_( node_id )
  , peer_id_( peer_id )
  , crypto_( move( crypto ) )
  , auto_home_( true )
  , destination_()
{}

void NetworkConnection::send_packet( UDPSocket& socket )
{
  if ( not has_destination() ) {
    throw runtime_error( "no destination" );
  }

  /* make packet to send */
  Packet pack {};
  sender_.set_sender_section( pack.sender_section );
  receiver_.set_receiver_section( pack.receiver_section );

  /* serialize */
  Plaintext plaintext;
  Serializer s { plaintext };
  pack.serialize( s );
  plaintext.resize( s.bytes_written() );

  /* encrypt */
  Ciphertext ciphertext;
  crypto_.encrypt( { &node_id_, 1 }, plaintext, ciphertext );

  socket.sendto( destination_.value(), ciphertext );
}

bool NetworkConnection::receive_packet( const Ciphertext& ciphertext, const Address& source )
{
  if ( not receive_packet( ciphertext ) ) {
    return false;
  }

  /* rehome? */
  if ( auto_home_ ) {
    if ( not last_biggest_seqno_received_.has_value() ) {
      destination_ = source;
      last_biggest_seqno_received_ = receiver_.biggest_seqno_received();
    } else if ( receiver_.biggest_seqno_received() > last_biggest_seqno_received_.value() ) {
      destination_ = source;
      last_biggest_seqno_received_ = receiver_.biggest_seqno_received();
    }
  }

  return true;
}

bool NetworkConnection::receive_packet( const Ciphertext& ciphertext )
{
  /* decrypt */
  Plaintext plaintext;
  if ( not crypto_.decrypt( ciphertext, { &peer_id_, 1 }, plaintext ) ) {
    stats_.decryption_failures++;
    return false;
  }

  /* parse */
  Parser parser { plaintext };
  const Packet packet { parser };
  if ( parser.error() ) {
    stats_.invalid++;
    parser.clear_error();
    return false;
  }

  /* act on packet contents */
  sender_.receive_receiver_section( packet.receiver_section );
  receiver_.receive_sender_section( packet.sender_section );

  return true;
}

void NetworkConnection::summary( ostream& out ) const
{
  if ( stats_.decryption_failures ) {
    out << "decryption_failures=" << stats_.decryption_failures << " ";
  }

  if ( stats_.invalid ) {
    out << "invalid=" << stats_.invalid << " ";
  }

  sender_.summary( out );
  receiver_.summary( out );
}
