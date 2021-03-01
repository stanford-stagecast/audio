#pragma once

#include <memory>

#include "raster.hh"

extern "C"
{
#include "libswscale/swscale.h"
}

class Scaler
{
  static constexpr uint16_t input_width = 3840, input_height = 2160;
  static constexpr uint16_t output_width = 1280, output_height = 720;

  SwsContext* context_ { nullptr };

  uint16_t source_x_ { 0 }, source_y_ { 0 }, source_width_ { input_width }, source_height_ { input_height };

  void saturate_params();

  bool need_new_context_ { false };
  void create_context();

public:
  Scaler() { create_context(); }
  ~Scaler()
  {
    if ( context_ ) {
      sws_freeContext( context_ );
    }
  }

  void setup( const uint16_t x, const uint16_t y, const uint16_t width, const uint16_t height );

  void scale( const RasterYUV422& source, RasterYUV420& dest );

  Scaler( const Scaler& other ) = delete;
  Scaler& operator=( const Scaler& other ) = delete;
};

class ColorspaceConverter
{
  SwsContext* yuv2rgba_ { nullptr };
  SwsContext* rgba2yuv_ { nullptr };

public:
  ColorspaceConverter( const uint16_t width, const uint16_t height );
  ~ColorspaceConverter();

  void convert( const RasterYUV420& yuv, RasterRGBA& output );
  void convert( const RasterRGBA& rgba, RasterYUV420& output );

  ColorspaceConverter( ColorspaceConverter&& other ) = default;
  ColorspaceConverter& operator=( ColorspaceConverter&& other ) = default;

  ColorspaceConverter( const ColorspaceConverter& other ) = delete;
  ColorspaceConverter& operator=( const ColorspaceConverter& other ) = delete;
};
