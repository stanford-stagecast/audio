#pragma once

#include "audio_buffer.hh"
#include "opus.hh"

#include <optional>

class OpusDecoderProcess
{
  OpusDecoder dec1_;
  std::optional<OpusDecoder> dec2_;

  struct Statistics
  {
    unsigned int ignored_decodes, successful_decodes, missing_decodes;
  } stats_ {};

public:
  OpusDecoderProcess( const bool independent_channels );

  void decode( const opus_frame& ch1,
               const opus_frame& ch2,
               const size_t global_sample_index,
               ChannelPair& output );

  void decode_stereo( const opus_frame& frame, const size_t global_sample_index, ChannelPair& output );

  void decode_missing( const size_t global_sample_index, ChannelPair& output );

  const Statistics& stats() const { return stats_; }
};
