/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#pragma once

#include <fstream>
#include <x264.h>

#include "raster.hh"

struct EncodedData
{
  uint8_t* payload;
  int frame_size;
};

class H264Encoder
{
private:
  x264_t* encoder_ {};
  x264_param_t params_ {};
  x264_picture_t pic_in_ {};
  x264_picture_t pic_out_ {};
  x264_nal_t* nal_ {};
  int frame_size_ {};

  uint16_t width_;
  uint16_t height_;
  uint8_t fps_;
  uint32_t frame_num_ {};
  // For writing encoded data to file
  std::ofstream stream_out_ {};

  void load_frame( BaseRaster& raster );
  H264Encoder( const H264Encoder& ) = delete;
  H264Encoder& operator=( const H264Encoder& ) = delete;

public:
  H264Encoder( const uint16_t width,
               const uint16_t height,
               const uint8_t fps,
               const std::string& preset,
               const std::string& tune );
  ~H264Encoder();
  bool encode( BaseRaster& raster );
  EncodedData get_encoded_data( void ) { return { nal_[0].p_payload, frame_size_ }; }
  void create_output_file( const std::string& filename );
  void write_to_file( const EncodedData& data );
  void close_output_file( void );
};
