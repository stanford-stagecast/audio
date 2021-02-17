#include "decoder_process.hh"
#include "spans.hh"

using namespace std;

OpusDecoderProcess::OpusDecoderProcess( const bool independent_channels )
  : dec1_( 48000, independent_channels ? 1 : 2 )
  , dec2_( independent_channels ? make_optional<OpusDecoder>( 48000, 1 ) : nullopt )
{}

void OpusDecoderProcess::decode( const opus_frame& ch1,
                                 const opus_frame& ch2,
                                 span<float> ch1_out,
                                 span<float> ch2_out )
{
  dec1_.decode( ch1, ch1_out );
  dec2_.value().decode( ch2, ch2_out );
}

void OpusDecoderProcess::decode_stereo( const opus_frame& frame, span<float> ch1_out, span<float> ch2_out )
{
  dec1_.decode_stereo( frame, ch1_out, ch2_out );
  if ( dec2_.has_value() ) {
    throw runtime_error( "OpusDecoderProcess::decode_stereo called on independent-channel decoder" );
  }
}

void OpusDecoderProcess::decode_missing( span<float> ch1_out, span<float> ch2_out )
{
  if ( dec2_.has_value() ) {
    dec1_.decode_missing( ch1_out );
    dec2_->decode_missing( ch2_out );
  } else {
    dec1_.decode_missing_stereo( ch1_out, ch2_out );
  }
}
