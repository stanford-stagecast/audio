#include <array>
#include <iostream>

#include "connection.hh"
#include "video_source.hh"

using namespace std;

template<class FrameType, class SourceType>
NetworkConnection<FrameType, SourceType>::NetworkConnection( const char node_id,
                                                             const char peer_id,
                                                             CryptoSession&& crypto,
                                                             const Address& destination )
  : NetworkConnection( node_id, peer_id, move( crypto ) )
{
  auto_home_ = false;
  destination_.emplace( destination );
}

template<class FrameType, class SourceType>
NetworkConnection<FrameType, SourceType>::NetworkConnection( const char node_id,
                                                             const char peer_id,
                                                             CryptoSession&& crypto )
  : node_id_( node_id )
  , peer_id_( peer_id )
  , crypto_( move( crypto ) )
  , auto_home_( true )
  , destination_()
{}

template<class FrameType, class SourceType>
void NetworkConnection<FrameType, SourceType>::send_packet( UDPSocket& socket )
{
  if ( not has_destination() ) {
    throw runtime_error( "no destination" );
  }

  /* make packet to send */
  Packet<FrameType> pack {};
  sender_.set_sender_section( pack.sender_section );
  receiver_.set_receiver_section( pack.receiver_section );

  /* do we have room for an unreliable update? */
  if ( pending_outbound_unreliable_data_.has_value() and ( pack.serialized_length() < 1200 ) ) {
    pack.unreliable_data_ = pending_outbound_unreliable_data_.value();
    pending_outbound_unreliable_data_.reset();
  }

  /* serialize */
  Plaintext plaintext;
  Serializer s { plaintext.mutable_buffer() };
  pack.serialize( s );
  plaintext.resize( s.bytes_written() );

  /* encrypt */
  Ciphertext ciphertext;
  crypto_.encrypt( { &node_id_, 1 }, plaintext, ciphertext );

  socket.sendto( destination_.value(), ciphertext );
}

template<class FrameType, class SourceType>
bool NetworkConnection<FrameType, SourceType>::receive_packet( const Ciphertext& ciphertext, const Address& source )
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

template<class FrameType, class SourceType>
bool NetworkConnection<FrameType, SourceType>::receive_packet( const Ciphertext& ciphertext )
{
  /* decrypt */
  Plaintext plaintext;
  if ( not crypto_.decrypt( ciphertext, { &peer_id_, 1 }, plaintext ) ) {
    stats_.decryption_failures++;
    return false;
  }

  /* parse */
  Parser parser { plaintext };
  const Packet<FrameType> packet { parser };
  if ( parser.error() ) {
    stats_.invalid++;
    parser.clear_error();
    return false;
  }

  if ( packet.sender_section.sequence_number == uint32_t( -1 ) ) { /* ignore packet, only used for priming */
    return true;
  }

  /* act on packet contents */
  sender_.receive_receiver_section( packet.receiver_section );
  receiver_.receive_sender_section( packet.sender_section );

  if ( packet.unreliable_data_.length() > 0 ) {
    inbound_unreliable_data_.emplace( packet.unreliable_data_ );
  }

  return true;
}

template<class FrameType, class SourceType>
void NetworkConnection<FrameType, SourceType>::summary( ostream& out ) const
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

template class NetworkConnection<AudioFrame, OpusEncoderProcess>;
