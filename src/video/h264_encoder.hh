#pragma once

#include <memory>
#include <optional>

#include <x264.h>

#include "formats.hh"
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
  x264_picture_t pic_in_ {}, pic_out_ {};
  int frame_size_ {};

  uint16_t width_;
  uint16_t height_;
  uint8_t fps_;
  uint32_t frame_num_ {};

public:
  struct EncodedNAL
  {
    span<uint8_t> NAL;
    int64_t pts;
    int64_t dts;
  };

private:
  std::optional<EncodedNAL> encoded_ {};

public:
  H264Encoder( const uint16_t width,
               const uint16_t height,
               const uint8_t fps,
               const std::string& preset,
               const std::string& tune );

  void encode( RasterYUV420& raster );

  bool has_nal() const { return encoded_.has_value(); }

  const EncodedNAL& nal() const { return encoded_.value(); }

  void reset_nal() { encoded_.reset(); }
};
