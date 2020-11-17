#include <array>
#include <iostream>

#include "connection.hh"

using namespace std;

NetworkEndpoint::NetworkEndpoint()
  : socket_()
{
  socket_.set_blocking( false );
}

NetworkClient::NetworkClient( const Address& server, std::shared_ptr<OpusEncoderProcess> source, EventLoop& loop )
  : server_( server )
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
  cerr << "Bound to: " << socket_.local_address().to_string() << "\n";

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

  /* serialize and send */
  array<char, 1400> packet_buf;
  Serializer s { { packet_buf.data(), packet_buf.size() } };
  pack.serialize( s );

  socket_.sendto( dest, { packet_buf.data(), s.bytes_written() } );
}

Address NetworkEndpoint::receive_packet()
{
  /* receive the packet */
  Address src { nullptr, 0 };
  array<char, 65536> packet_buf;
  string_span payload { packet_buf.data(), packet_buf.size() };
  socket_.recv( src, payload );

  /* parse it */
  Parser parser { payload };
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
    out << "Invalid packets: " << stats_.invalid << "\n";
  }
  sender_.generate_statistics( out );
  receiver_.generate_statistics( out );
}
