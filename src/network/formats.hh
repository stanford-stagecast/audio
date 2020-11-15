#pragma once

#include <cstdint>
#include <vector>

#include "opus.hh"
#include "spans.hh"

struct AudioFrame
{
  uint32_t frame_index {}; // units of opus_frame::NUM_SAMPLES, about four months at 2^32 * 120 / 48 kHz
  opus_frame ch1 {}, ch2 {};

  uint8_t serialized_length() const;
  uint8_t serialize( const string_span s ) const;
  uint8_t parse( const std::string_view s );
  size_t sample_index() const { return frame_index * opus_frame::NUM_SAMPLES; }
};

struct Packet
{
  uint8_t num_frames {};
  std::array<AudioFrame, 8> frames {};

  uint16_t serialized_length() const;
  uint16_t serialize( const string_span s ) const;
  uint16_t parse( const std::string_view s );
};
