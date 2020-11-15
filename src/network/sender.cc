#include <iostream>

#include "sender.hh"

using namespace std;

NetworkSender::NetworkSender( const Address& server, shared_ptr<OpusEncoderProcess> encoder, EventLoop& loop )
  : server_( server )
  , encoder_( encoder )
{
  loop.add_rule(
    "network transmit", [&] { push_one_frame( *encoder_ ); }, [&] { return encoder_->has_frame(); } );
}

void NetworkSender::push_one_frame( OpusEncoderProcess& encoder )
{
  if ( encoder.frame_index() < frames_outstanding_.range_begin() ) {
    throw runtime_error( "NetworkSender: encoder cursor is too early" );
  }

  if ( encoder.frame_index() >= frames_outstanding_.range_end() ) {
    /* must drop some old frames */
    const size_t frames_to_drop = encoder.frame_index() - frames_outstanding_.range_end() + 1;
    frames_outstanding_.pop( frames_to_drop );
    frames_dropped_ += frames_to_drop;
  }

  if ( frames_outstanding_.at( encoder.frame_index() ).has_value() ) {
    throw runtime_error( "NetworkSender: frame submitted multiple times" );
  }

  frames_outstanding_.at( encoder.frame_index() )
    = { uint32_t( encoder.frame_index() ), encoder.front_ch1(), encoder.front_ch2() };

  encoder.pop_frame();
}

void NetworkSender::generate_statistics( ostringstream& out )
{
  out << "Sender info:";
  if ( frames_dropped_ ) {
    out << " dropped=" << frames_dropped_;
  }
  out << "\n";
}
