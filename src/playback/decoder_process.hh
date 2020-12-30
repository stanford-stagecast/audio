#pragma once

#include "audio_buffer.hh"
#include "opus.hh"

class OpusDecoderProcess
{
  AudioBuffer output_ { 65536 };
  OpusDecoder dec1 { 48000 }, dec2 { 48000 };

  struct Statistics
  {
    unsigned int ignored_decodes, successful_decodes, missing_decodes;
  } stats_ {};

public:
  void decode( const opus_frame& ch1, const opus_frame& ch2, const size_t global_sample_index );
  void decode_missing( const size_t global_sample_index );

  const Statistics& stats() const { return stats_; }
  const AudioBuffer& output() const { return output_; }
  AudioBuffer& output() { return output_; }
};
