#include "decoder_process.hh"
#include "spans.hh"

using namespace std;

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

  dec1.decode( ch1, output.ch1().region( global_sample_index, opus_frame::NUM_SAMPLES_MINLATENCY ) );
  dec2.decode( ch2, output.ch2().region( global_sample_index, opus_frame::NUM_SAMPLES_MINLATENCY ) );

  stats_.successful_decodes++;
}

void OpusDecoderProcess::decode_missing( const size_t global_sample_index, ChannelPair& output )
{
  if ( global_sample_index < output.range_begin()
       or global_sample_index + opus_frame::NUM_SAMPLES_MINLATENCY >= output.range_end() ) {
    stats_.ignored_decodes++;
    return;
  }

  dec1.decode_missing( output.ch1().region( global_sample_index, opus_frame::NUM_SAMPLES_MINLATENCY ) );
  dec2.decode_missing( output.ch2().region( global_sample_index, opus_frame::NUM_SAMPLES_MINLATENCY ) );

  stats_.missing_decodes++;
}
