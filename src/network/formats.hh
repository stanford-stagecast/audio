#pragma once

#include <cstdint>

#include "opus.hh"
#include "spans.hh"

struct AudioMessage
{
  uint32_t frame_index {}; // units of opus_frame::NUM_SAMPLES, about four months at 2^32 * 120 / 48 kHz
  opus_frame ch1 {}, ch2 {};

  uint8_t serialized_length() const;
  void serialize( const string_span s );
  bool parse( const std::string_view s );
};
