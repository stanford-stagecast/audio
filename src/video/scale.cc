#include <iostream>

#include "exception.hh"
#include "scale.hh"

using namespace std;

void Scaler::create_context()
{
  context_ = notnull( "sws_getCachedContext",
                      sws_getCachedContext( context_,
                                            source_width_,
                                            source_height_,
                                            AV_PIX_FMT_YUV422P,
                                            output_width,
                                            output_height,
                                            AV_PIX_FMT_YUV420P,
                                            SWS_BICUBIC,
                                            nullptr,
                                            nullptr,
                                            nullptr ) );

  need_new_context_ = false;
}

void Scaler::setup( const uint16_t x, const uint16_t y, const uint16_t width, const uint16_t height )
{
  if ( width != source_width_ or height != source_height_ ) {
    need_new_context_ = true;
  }

  source_x_ = x;
  source_y_ = y;
  source_width_ = width;
  source_height_ = height;

  saturate_params();

  if ( need_new_context_ ) {
    create_context();
  }
}

void Scaler::saturate_params()
{
  if ( ( source_x_ + source_width_ > input_width ) or ( source_y_ + source_height_ > input_height )
       or source_width_ == 0 or source_height_ == 0 ) {
    source_x_ = source_y_ = 0;
    source_width_ = input_width;
    source_height_ = input_height;
    need_new_context_ = true;
    cerr << "Error, invalid parameters.\n";
  }
}

void Scaler::scale( const RasterYUV422& source, RasterYUV420& dest )
{
  if ( source.width() != input_width or source.height() != input_height ) {
    throw runtime_error( "source size mismatch" );
  }

  if ( dest.width() != output_width or dest.height() != output_height ) {
    throw runtime_error( "dest size mismatch" );
  }

  const array<const uint8_t*, 3> source_planes { source.Y().data() + source_y_ * input_width + source_x_,
                                                 source.Cb().data() + source_y_ * input_width / 2 + source_x_ / 2,
                                                 source.Cr().data() + source_y_ * input_width / 2 + source_x_ / 2 };

  const array<uint8_t*, 3> dest_planes { dest.Y_row( 0 ), dest.Cb_row( 0 ), dest.Cr_row( 0 ) };

  const array<const int, 3> source_strides { input_width, input_width / 2, input_width / 2 };
  const array<const int, 3> dest_strides { output_width, output_width / 2, output_width / 2 };

  if ( not context_ ) {
    throw runtime_error( "null ptr!" );
  }

  const int rows_written = sws_scale( context_,
                                      source_planes.data(),
                                      source_strides.data(),
                                      0,
                                      source_height_,
                                      dest_planes.data(),
                                      dest_strides.data() );

  if ( rows_written != output_height ) {
    throw runtime_error( "unexpected return value from sws_scale(): " + to_string( rows_written ) );
  }
}

ColorspaceConverter::ColorspaceConverter( const uint16_t width, const uint16_t height )
{
  yuv2rgba_ = notnull( "sws_getCachedContext Y'CbCr => R'G'B'A",
                       sws_getCachedContext( yuv2rgba_,
                                             width,
                                             height,
                                             AV_PIX_FMT_YUV420P,
                                             width,
                                             height,
                                             AV_PIX_FMT_RGBA,
                                             SWS_BICUBIC,
                                             nullptr,
                                             nullptr,
                                             nullptr ) );

  rgba2yuv_ = notnull( "sws_getCachedContext R'G'B'A => Y'CbCr",
                       sws_getCachedContext( rgba2yuv_,
                                             width,
                                             height,
                                             AV_PIX_FMT_RGBA,
                                             width,
                                             height,
                                             AV_PIX_FMT_YUV420P,
                                             SWS_BICUBIC,
                                             nullptr,
                                             nullptr,
                                             nullptr ) );
}

ColorspaceConverter::~ColorspaceConverter()
{
  if ( yuv2rgba_ ) {
    sws_freeContext( yuv2rgba_ );
  }

  if ( rgba2yuv_ ) {
    sws_freeContext( rgba2yuv_ );
  }
}

void ColorspaceConverter::convert( const RasterYUV420& yuv, RasterRGBA& output )
{
  const array<const uint8_t*, 3> source_planes { yuv.Y_row( 0 ), yuv.Cb_row( 0 ), yuv.Cr_row( 0 ) };
  const array<const int, 3> source_strides { yuv.width(), yuv.chroma_width(), yuv.chroma_width() };

  const array<uint8_t*, 3> dest_planes { output.data(), nullptr, nullptr };
  const array<const int, 3> dest_strides { output.width() * 4, 0, 0 };

  sws_scale( yuv2rgba_,
             source_planes.data(),
             source_strides.data(),
             0,
             yuv.height(),
             dest_planes.data(),
             dest_strides.data() );
}

void ColorspaceConverter::convert( const RasterRGBA& rgba, RasterYUV420& output )
{
  const array<const uint8_t*, 3> source_planes { rgba.data(), nullptr, nullptr };
  const array<const int, 3> source_strides { rgba.width() * 4, 0, 0 };

  const array<uint8_t*, 3> dest_planes { output.Y_row( 0 ), output.Cb_row( 0 ), output.Cr_row( 0 ) };
  const array<const int, 3> dest_strides { output.width(), output.chroma_width(), output.chroma_width() };

  sws_scale( rgba2yuv_,
             source_planes.data(),
             source_strides.data(),
             0,
             output.height(),
             dest_planes.data(),
             dest_strides.data() );
}
