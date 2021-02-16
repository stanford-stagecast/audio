#pragma once

#include "audio_buffer.hh"
#include "opus.hh"

class OpusDecoderProcess
{
  OpusDecoder dec1 { 48000 }, dec2 { 48000 };

  struct Statistics
  {
    unsigned int ignored_decodes, successful_decodes, missing_decodes;
  } stats_ {};

public:
  void decode( const opus_frame& ch1,
               const opus_frame& ch2,
               const size_t global_sample_index,
               ChannelPair& output );
  void decode_missing( const size_t global_sample_index, ChannelPair& output );

  const Statistics& stats() const { return stats_; }
};
