#include "vsclient.hh"

using namespace std;
using namespace chrono;

using Option = RubberBand::RubberBandStretcher::Option;

VSClient::VSClient( const uint8_t node_id, CryptoSession&& crypto )
  : connection_( 0, node_id, move( crypto ) )
{}

bool VSClient::receive_packet( const Address& source, const Ciphertext& ciphertext )
{
  const bool ret = connection_.receive_packet( ciphertext, source );

  while ( connection_.next_frame_needed() > connection_.frames().range_begin() ) {
    const VideoChunk& chunk = connection_.frames().at( connection_.frames().range_begin() ).value();
    const size_t new_size = current_nal_.length() + chunk.data.length();
    if ( new_size + AV_INPUT_BUFFER_PADDING_SIZE > current_nal_.capacity() ) {
      throw runtime_error( "NAL too big" );
    }

    memcpy( current_nal_.mutable_data_ptr() + current_nal_.length(), chunk.data.data_ptr(), chunk.data.length() );
    current_nal_.resize( new_size );

    if ( chunk.end_of_nal ) {
      decoder_.decode( current_nal_.as_string_view(), raster_ );
      NALs_decoded_++;
      current_nal_.resize( 0 );
    }

    connection_.pop_frames( 1 );
  }

  return ret;
}

void VSClient::send_packet( UDPSocket& socket )
{
  if ( connection_.has_destination() ) {
    connection_.send_packet( socket );
  }
}

void VSClient::summary( ostream& out ) const
{
  if ( connection_.has_destination() ) {
    out << " (" << connection_.destination().to_string() << ") ";
  }
  out << "video frames decoded: " << NALs_decoded_ << "\n";
  connection_.summary( out );
}

void KnownVideoClient::summary( ostream& out ) const
{
  out << name_ << ":";
  out << " requests=" << stats_.key_requests;
  out << " responses=" << stats_.key_responses;
  out << " new_sessions=" << stats_.new_sessions;
  if ( current_session_.has_value() ) {
    current_session_->summary( out );
  }
}

bool KnownVideoClient::try_keyrequest( const Address& src, const Ciphertext& ciphertext, UDPSocket& socket )
{
  Plaintext plaintext;
  if ( long_lived_crypto_.decrypt( ciphertext, { &KeyMessage::keyreq_id, 1 }, plaintext )
       and ( plaintext.length() == 0 ) ) {
    stats_.key_requests++;
    if ( steady_clock::now() < next_reply_allowed_ ) {
      return true;
    }

    /* reply with keys to next session */
    Plaintext outgoing_keys;
    {
      Serializer s { outgoing_keys.mutable_buffer() };
      s.object( KeyMessage { id_, next_keys_ } );
      outgoing_keys.resize( s.bytes_written() );
    }
    Ciphertext outgoing_ciphertext;
    long_lived_crypto_.encrypt( { &KeyMessage::keyreq_server_id, 1 }, outgoing_keys, outgoing_ciphertext );
    socket.sendto( src, outgoing_ciphertext );
    next_reply_allowed_ = steady_clock::now() + milliseconds( 250 );

    stats_.key_responses++;
    return true;
  }
  return false;
}

KnownVideoClient::KnownVideoClient( const uint8_t node_id, const LongLivedKey& key )
  : id_( node_id )
  , name_( key.name() )
  , long_lived_crypto_( key.key_pair().downlink, key.key_pair().uplink, true )
  , next_reply_allowed_( steady_clock::now() )
  , next_session_( CryptoSession { next_keys_.downlink, next_keys_.uplink } )
{}

void KnownVideoClient::receive_packet( const Address& src,
                                       const Ciphertext& ciphertext,
                                       const uint64_t clock_sample __attribute( ( unused ) ) )
{
  if ( current_session_.has_value() and current_session_->receive_packet( src, ciphertext ) ) {
    return;
  }

  Plaintext throwaway_plaintext;
  if ( next_session_.value().decrypt( ciphertext, { &id_, 1 }, throwaway_plaintext ) ) {
    /* new session established */
    current_session_.emplace( id_, move( next_session_.value() ) );

    next_keys_ = KeyPair {};
    next_session_.emplace( next_keys_.downlink, next_keys_.uplink );
    stats_.new_sessions++;

    /* actually use packet */
    current_session_->receive_packet( src, ciphertext );
  }
}
