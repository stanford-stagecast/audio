#include <array>
#include <iostream>

#include "connection.hh"

using namespace std;

NetworkClient::NetworkClient( const Address& server,
                              const Base64Key& send_key,
                              const Base64Key& receive_key,
                              std::shared_ptr<OpusEncoderProcess> source,
                              EventLoop& loop )
  : socket_()
  , server_( server )
  , source_( source )
  , crypto_( send_key, receive_key )
{
  socket_.set_blocking( false );

  loop.add_rule(
    "network transmit",
    [&] {
      push_frame( *source_ );
      send_packet( crypto_, server_, socket_ );
    },
    [&] { return source_->has_frame(); } );

  loop.add_rule( "network receive", socket_, Direction::In, [&] {
    Address src { nullptr, 0 };
    Ciphertext ciphertext;
    socket_.recv( src, ciphertext.data );

    /* decrypt */
    Plaintext plaintext;
    if ( not crypto_.decrypt( ciphertext, plaintext ) ) {
      decryption_failure();
      return;
    }

    receive_packet( plaintext );
  } );
}

NetworkSingleServer::NetworkSingleServer( EventLoop& loop )
  : socket_ {}
  , peer_ { nullptr, 0 }
{
  socket_.set_blocking( false );
  socket_.bind( { "0", 0 } );
  cout << "Port " << socket_.local_address().port() << " " << receive_key_.printable_key().as_string_view() << " "
       << send_key_.printable_key().as_string_view() << endl;

  loop.add_rule( "network receive", socket_, Direction::In, [&] {
    Ciphertext ciphertext;
    socket_.recv( peer_, ciphertext.data );

    /* decrypt */
    Plaintext plaintext;
    if ( not crypto_.decrypt( ciphertext, plaintext ) ) {
      decryption_failure();
      return;
    }

    receive_packet( plaintext );
    pop_frames( next_frame_needed() - frames().range_begin() );
    send_packet( crypto_, peer_, socket_ );
  } );
}

void NetworkEndpoint::push_frame( OpusEncoderProcess& source )
{
  sender_.push_frame( source );
}

void NetworkEndpoint::send_packet( Session& crypto_session, const Address& dest, UDPSocket& socket )
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
  crypto_session.encrypt( plaintext, ciphertext );

  socket.sendto( dest, ciphertext );
}

void NetworkEndpoint::receive_packet( Plaintext& plaintext )
{
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
}

void NetworkEndpoint::summary( ostream& out ) const
{
  if ( stats_.decryption_failures ) {
    out << "decryption_failures=" << stats_.decryption_failures;
  }

  if ( stats_.invalid ) {
    out << "invalid=" << stats_.invalid << " ";
  }

  out << "\n";

  sender_.generate_statistics( out );
  receiver_.generate_statistics( out );
}
