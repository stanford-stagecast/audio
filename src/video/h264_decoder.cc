#include "h264_decoder.hh"
#include "av_check.hh"
#include "exception.hh"

#include <iostream>
#include <memory>

using namespace std;

H264Decoder::H264Decoder()
  : codec_( notnull( "avcodec_find_decoder", avcodec_find_decoder( AV_CODEC_ID_H264 ) ) )
  , context_( notnull( "avcodec_alloc_context3", avcodec_alloc_context3( codec_ ) ) )
{
  av_log_set_level( AV_LOG_QUIET );
  av_check( avcodec_open2( context_.get(), codec_, nullptr ) );
}

bool H264Decoder::decode( const span_view<uint8_t> nal, RasterYUV420& output __attribute( ( unused ) ) )
{
  AVPacket packet {};
  packet.data = const_cast<uint8_t*>( nal.data() );
  packet.size = nal.size();

  if ( avcodec_send_packet( context_.get(), &packet ) < 0 ) {
    return false;
  }

  const auto receive_ret = avcodec_receive_frame( context_.get(), frame_.frame );
  if ( receive_ret == AVERROR( EAGAIN ) ) {
    cerr << "again!\n";
    return false;
  } else if ( receive_ret ) {
    av_check( receive_ret );
    return false;
  }

  return true;
}

H264Decoder::H264Decoder( H264Decoder&& other ) noexcept
  : codec_( other.codec_ )
  , context_( move( other.context_ ) )
  , frame_( move( other.frame_ ) )
{}
