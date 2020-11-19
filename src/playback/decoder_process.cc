#include "decoder_process.hh"
#include "spans.hh"

using namespace std;

void OpusDecoderProcess::decode( const opus_frame& ch1, const opus_frame& ch2, const size_t global_sample_index )
{
  if ( global_sample_index < range_begin() or global_sample_index + opus_frame::NUM_SAMPLES >= range_end() ) {
    stats_.ignored_decodes++;
    return;
  }

  span<float> ch1_samples = ch1_.output.region( global_sample_index, opus_frame::NUM_SAMPLES );
  span<float> ch2_samples = ch2_.output.region( global_sample_index, opus_frame::NUM_SAMPLES );

  ch1_.dec.decode( ch1, ch1_samples );
  ch2_.dec.decode( ch2, ch2_samples );
}
