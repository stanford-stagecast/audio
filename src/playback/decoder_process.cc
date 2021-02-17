#include "decoder_process.hh"
#include "spans.hh"

using namespace std;

OpusDecoderProcess::OpusDecoderProcess( const bool independent_channels )
  : dec1_( 48000, independent_channels ? 1 : 2 )
  , dec2_( independent_channels ? make_optional<OpusDecoder>( 48000, 1 ) : nullopt )
{}

void OpusDecoderProcess::decode( const opus_frame& ch1,
                                 const opus_frame& ch2,
                                 const size_t global_sample_index,
                                 ChannelPair& output )
{
  if ( global_sample_index < output.range_begin()
       or global_sample_index + opus_frame::NUM_SAMPLES_MINLATENCY >= output.range_end() ) {
    stats_.ignored_decodes++;
    return;
  }

  dec1_.decode( ch1, output.ch1().region( global_sample_index, opus_frame::NUM_SAMPLES_MINLATENCY ) );
  dec2_.value().decode( ch2, output.ch2().region( global_sample_index, opus_frame::NUM_SAMPLES_MINLATENCY ) );

  stats_.successful_decodes++;
}

void OpusDecoderProcess::decode_stereo( const opus_frame& frame,
                                        const size_t global_sample_index,
                                        ChannelPair& output )
{
  if ( global_sample_index < output.range_begin()
       or global_sample_index + opus_frame::NUM_SAMPLES_MINLATENCY >= output.range_end() ) {
    stats_.ignored_decodes++;
    return;
  }

  dec1_.decode_stereo( frame,
                       output.ch1().region( global_sample_index, opus_frame::NUM_SAMPLES_MINLATENCY ),
                       output.ch2().region( global_sample_index, opus_frame::NUM_SAMPLES_MINLATENCY ) );
  if ( dec2_.has_value() ) {
    throw runtime_error( "OpusDecoderProcess::decode_stereo called on independent-channel decoder" );
  }

  stats_.successful_decodes++;
}

void OpusDecoderProcess::decode_missing( const size_t global_sample_index, ChannelPair& output )
{
  if ( global_sample_index < output.range_begin()
       or global_sample_index + opus_frame::NUM_SAMPLES_MINLATENCY >= output.range_end() ) {
    stats_.ignored_decodes++;
    return;
  }

  if ( dec2_.has_value() ) {
    dec1_.decode_missing( output.ch1().region( global_sample_index, opus_frame::NUM_SAMPLES_MINLATENCY ) );
    dec2_->decode_missing( output.ch2().region( global_sample_index, opus_frame::NUM_SAMPLES_MINLATENCY ) );
  } else {
    dec1_.decode_missing_stereo( output.ch1().region( global_sample_index, opus_frame::NUM_SAMPLES_MINLATENCY ),
                                 output.ch2().region( global_sample_index, opus_frame::NUM_SAMPLES_MINLATENCY ) );
  }

  stats_.missing_decodes++;
}
