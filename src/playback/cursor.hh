#pragma once

#include <cstdint>

#include "connection.hh"

class Cursor
{
  uint64_t initial_delay_ns_;

  bool initialized {};
  uint64_t next_frame_index_ts_ {};
  uint32_t next_frame_index_ {};

  static constexpr float ALPHA = 1 / 100.0;
  float quality = 1.0;
  float average_safety_margin {};

  static bool has_frame( const NetworkEndpoint& endpoint, const uint32_t frame_index );

public:
  Cursor( const uint64_t initial_delay_ns )
    : initial_delay_ns_( initial_delay_ns )
  {}

  void sample( const NetworkEndpoint& endpoint, const uint64_t now );
};
