#pragma once

#include <memory>
#include <x264.h>

#include "raster.hh"
#include "spans.hh"

class H264Encoder
{
private:
  struct x264_deleter
  {
    void operator()( x264_t* x ) const { x264_encoder_close( x ); }
  };

  std::unique_ptr<x264_t, x264_deleter> encoder_ {};
  x264_param_t params_ {};
  x264_picture_t pic_in_ {};
  int frame_size_ {};

  uint16_t width_;
  uint16_t height_;
  uint8_t fps_;
  uint32_t frame_num_ {};

public:
  H264Encoder( const uint16_t width, const uint16_t height, const uint8_t fps, const std::string& preset );

  std::string_view encode( RasterYUV420& raster );
};
