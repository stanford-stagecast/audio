/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include <iostream>

#include "h264_encoder.hh"

using namespace std;

H264Encoder::H264Encoder( const uint16_t width,
                          const uint16_t height,
                          const uint8_t fps,
                          const string& preset,
                          const string& tune )
  : width_( width )
  , height_( height )
  , fps_( fps )
{
  if ( x264_param_default_preset( &params_, preset.c_str(), tune.c_str() ) != 0 ) {
    throw runtime_error( "Error: Failed to set preset on x264." );
  }
  // Set params for encoder
  params_.i_threads = 1;
  params_.i_width = width_;
  params_.i_height = height_;
  params_.i_fps_num = fps_;
  params_.i_fps_den = 1;
  params_.b_annexb = 1;
  params_.b_repeat_headers = 1;
  params_.i_keyint_max = fps_;
  // Apply profile
  if ( x264_param_apply_profile( &params_, "baseline" ) != 0 ) {
    throw runtime_error( "Error: Failed to set baseline profile on x264." );
  }
  // Init encoder
  encoder_ = x264_encoder_open( &params_ );
  if ( !encoder_ ) {
    throw runtime_error( "Error: Failed to create the x264 encoder." );
  }
}

H264Encoder::~H264Encoder()
{
  if ( stream_out_.is_open() ) {
    close_output_file();
  }
}

void H264Encoder::load_frame( BaseRaster& raster )
{
  x264_picture_init( &pic_in_ );
  pic_in_.img.i_csp = X264_CSP_I420;
  pic_in_.img.i_plane = 3;
  pic_in_.img.i_stride[0] = width_;
  pic_in_.img.i_stride[1] = width_ / 2;
  pic_in_.img.i_stride[2] = width_ / 2;

  pic_in_.img.plane[0] = &raster.Y().at( 0, 0 );
  pic_in_.img.plane[1] = &raster.U().at( 0, 0 );
  pic_in_.img.plane[2] = &raster.V().at( 0, 0 );

  pic_in_.i_pts = frame_num_;
  frame_num_++;
}

bool H264Encoder::encode( BaseRaster& raster )
{
  load_frame( raster );
  int nals_count = 0;
  frame_size_ = x264_encoder_encode( encoder_, &nal_, &nals_count, &pic_in_, &pic_out_ );

  if ( frame_size_ < 0 ) {
    cerr << "Error: x264_encoder_encode failed!" << endl;
    return false;
  }

  if ( !nal_ ) {
    cerr << "Error: x264_encoder_encode returned no valid nals." << endl;
    return false;
  }
  return true;
}

void H264Encoder::create_output_file( const string& filename )
{
  if ( stream_out_.is_open() ) {
    throw runtime_error( "Error: Failed to open the video encoder output file "
                         "becuase it's already open." );
  }

  stream_out_.open( filename.c_str(), ios::binary | ios::out );
  if ( !stream_out_.is_open() ) {
    throw runtime_error( "Error: Failed to open the video encoder output file: " + filename );
  }
}

void H264Encoder::write_to_file( const EncodedData& data )
{
  if ( !stream_out_.is_open() ) {
    throw runtime_error( "Error: Failed to write to the video encoder output "
                         "file because the file hasn't been opened yet." );
  }
  stream_out_.write( reinterpret_cast<char*>( data.payload ), data.frame_size );
}

void H264Encoder::close_output_file( void )
{
  if ( !stream_out_.is_open() ) {
    throw runtime_error( "Error: Cannot close the encoder file because it "
                         "hasn't been opened yet." );
  }
  stream_out_.close();
}
