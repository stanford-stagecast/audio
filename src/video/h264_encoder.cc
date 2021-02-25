#include "h264_encoder.hh"
#include "exception.hh"

#include <iostream>

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
  params_.i_threads = 6;
  params_.i_width = width_;
  params_.i_height = height_;
  params_.i_fps_num = fps_;
  params_.i_fps_den = 1;
  params_.b_annexb = 1;
  params_.b_repeat_headers = 1;
  params_.i_keyint_max = fps_ * 5;
  //  params_.b_intra_refresh = true;

  params_.rc.i_qp_constant = 30;
  params_.rc.i_rc_method = X264_RC_CQP;

  // Apply profile
  if ( x264_param_apply_profile( &params_, "high" ) != 0 ) {
    throw runtime_error( "Error: Failed to set baseline profile on x264." );
  }
  encoder_.reset( notnull( "x264_encoder_open", x264_encoder_open( &params_ ) ) );

  x264_picture_init( &pic_in_ );
}

void H264Encoder::encode( RasterYUV420& raster )
{
  if ( has_nal() ) {
    throw runtime_error( "H264Encoder can't encode when still has NAL" );
  }

  if ( raster.width() != width_ or raster.height() != height_ ) {
    throw runtime_error( "H264Encoder::encode(): size mismatch" );
  }

  pic_in_.img.i_csp = X264_CSP_I420;
  pic_in_.img.i_plane = 3;
  pic_in_.img.i_stride[0] = raster.width();
  pic_in_.img.i_stride[1] = raster.chroma_width();
  pic_in_.img.i_stride[2] = raster.chroma_width();

  pic_in_.img.plane[0] = raster.Y_row( 0 );
  pic_in_.img.plane[1] = raster.Cb_row( 0 );
  pic_in_.img.plane[2] = raster.Cr_row( 0 );

  pic_in_.i_pts = 90000 * frame_num_ / fps_;
  frame_num_++;

  int nals_count = 0;
  x264_nal_t* nal;
  //  x264_encoder_intra_refresh( encoder_.get() );
  const auto frame_size = x264_encoder_encode( encoder_.get(), &nal, &nals_count, &pic_in_, &pic_out_ );

  if ( not nal or frame_size <= 0 ) {
    encoded_.reset();
  }

  encoded_.emplace( EncodedNAL { { nal->p_payload, size_t( frame_size ) }, pic_out_.i_pts, pic_out_.i_dts } );
}
