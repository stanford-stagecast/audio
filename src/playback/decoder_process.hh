#pragma once

#include "opus.hh"
#include "spans.hh"

#include <optional>

class OpusDecoderProcess
{
  OpusDecoder dec1_;
  std::optional<OpusDecoder> dec2_;

public:
  OpusDecoderProcess( const bool independent_channels );

  void decode( const opus_frame& ch1, const opus_frame& ch2, span<float> ch1_out, span<float> ch2_out );

  void decode_stereo( const opus_frame& frame, span<float> ch1_out, span<float> ch2_out );

  void decode_missing( span<float> ch1_out, span<float> ch2_out );
};
