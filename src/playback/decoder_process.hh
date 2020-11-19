#pragma once

#include "audio_buffer.hh"
#include "opus.hh"

class OpusDecoderProcess
{
  struct Channel
  {
    OpusDecoder dec { 48000 };
    AudioChannel output { 65536 };
  };

  Channel ch1_ {}, ch2_ {};

  struct Statistics
  {
    unsigned int ignored_decodes, successful_decodes;
  } stats_ {};

public:
  size_t range_begin() const { return ch1_.output.range_begin(); }
  size_t range_end() const { return ch1_.output.range_end(); }

  void pop( const size_t num_samples )
  {
    ch1_.output.pop( num_samples );
    ch2_.output.pop( num_samples );
  }

  void decode( const opus_frame& ch1, const opus_frame& ch2, const size_t global_sample_index );

  const Statistics& stats() const { return stats_; }
  const AudioChannel& ch1() const { return ch1_.output; }
  const AudioChannel& ch2() const { return ch1_.output; }
};
