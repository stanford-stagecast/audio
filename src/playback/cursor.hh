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
  float quality_ = 1.0;
  float average_safety_margin_ {};

  static bool has_frame( const NetworkEndpoint& endpoint, const uint32_t frame_index );

public:
  Cursor( const uint64_t initial_delay_ns )
    : initial_delay_ns_( initial_delay_ns )
  {}

  void sample( const NetworkEndpoint& endpoint, const uint64_t now );

  uint64_t initial_delay_ns() const { return initial_delay_ns_; }
  float quality() const { return quality_; }
  float safety_margin_ms() const { return average_safety_margin_; }
  uint32_t next_frame_index() const { return next_frame_index_; }
};
