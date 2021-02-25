#pragma once

#include "exception.hh"
#include "raster.hh"
#include "spans.hh"

#include <memory>

extern "C"
{
#include <libavcodec/avcodec.h>
}

struct FrameWrapper
{
  AVFrame* frame;

  FrameWrapper()
    : frame( notnull( "av_frame_alloc", av_frame_alloc() ) )
  {
    frame->buf[0] = av_buffer_alloc( 1024 * 1024 );
    frame->buf[1] = av_buffer_alloc( 1024 * 1024 );
    frame->buf[2] = av_buffer_alloc( 1024 * 1024 );
  }

  ~FrameWrapper()
  {
    if ( frame ) {
      av_frame_free( &frame );
    }
  }

  FrameWrapper( const FrameWrapper& other ) = delete;
  FrameWrapper& operator=( const FrameWrapper& other ) = delete;

  FrameWrapper( FrameWrapper&& other )
    : frame( other.frame )
  {
    other.frame = nullptr;
  }
};

class H264Decoder
{
  AVCodec* codec_;

  struct avcodec_deleter
  {
    void operator()( AVCodecContext* x ) const { av_free( x ); }
  };

  std::unique_ptr<AVCodecContext, avcodec_deleter> context_;
  FrameWrapper frame_ {};

public:
  H264Decoder();

  H264Decoder( const H264Decoder& other ) = delete;
  H264Decoder& operator=( const H264Decoder& other ) = delete;

  H264Decoder( H264Decoder&& other ) noexcept;

  bool decode( const span_view<uint8_t> nal, RasterYUV420& output );
};
