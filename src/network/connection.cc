#include <array>
#include <iostream>

#include "connection.hh"

using namespace std;

NetworkEndpoint::NetworkEndpoint()
  : socket_()
  , send_key_()
  , receive_key_()
{
  socket_.set_blocking( false );
}

NetworkEndpoint::NetworkEndpoint( const Base64Key& send_key, const Base64Key& receive_key )
  : socket_()
  , send_key_( send_key )
  , receive_key_( receive_key )
{
  socket_.set_blocking( false );
}

NetworkClient::NetworkClient( const Address& server,
                              const Base64Key& send_key,
                              const Base64Key& receive_key,
                              std::shared_ptr<OpusEncoderProcess> source,
                              EventLoop& loop )
  : NetworkEndpoint( send_key, receive_key )
  , server_( server )
  , source_( source )
{
  loop.add_rule(
    "network transmit",
    [&] {
      push_frame( *source_ );
      send_packet( server_ );
    },
    [&] { return source_->has_frame(); } );

  loop.add_rule( "network receive", socket_, Direction::In, [&] { receive_packet(); } );
}

NetworkServer::NetworkServer( EventLoop& loop )
  : peer_ {}
{
  socket_.bind( { "0", 0 } );
  cout << "Port " << socket_.local_address().port() << " " << receive_key_.printable_key().as_string_view() << " "
       << send_key_.printable_key().as_string_view() << endl;

  loop.add_rule( "network receive", socket_, Direction::In, [&] {
    peer_ = receive_packet();
    pop_frames( received_frames().size() );
    send_packet( peer_.value() );
  } );
}

void NetworkEndpoint::push_frame( OpusEncoderProcess& source )
{
  sender_.push_frame( source );
}

void NetworkEndpoint::send_packet( const Address& dest )
{
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
  crypto_.encrypt( plaintext, ciphertext );

  socket_.sendto( dest, ciphertext );
}

Address NetworkEndpoint::receive_packet()
{
  /* receive the packet */
  Address src { nullptr, 0 };
  Ciphertext ciphertext;
  socket_.recv( src, ciphertext.data );

  /* decrypt */
  Plaintext plaintext;
  crypto_.decrypt( ciphertext, plaintext );

  /* parse it */
  Parser parser { plaintext };
  const Packet packet { parser };
  if ( parser.error() ) {
    stats_.invalid++;
  } else {
    /* process it */
    sender_.receive_receiver_section( packet.receiver_section );
    receiver_.receive_sender_section( packet.sender_section );
  }

  return src;
}

void NetworkEndpoint::generate_statistics( ostream& out ) const
{
  if ( stats_.invalid ) {
    out << "invalid=" << stats_.invalid << " ";
  }

  if ( crypto_.decryption_failures() ) {
    out << "decryption_failures=" << crypto_.decryption_failures();
  }

  out << "\n";

  sender_.generate_statistics( out );
  receiver_.generate_statistics( out );
}
