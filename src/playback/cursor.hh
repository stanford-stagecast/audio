#pragma once

#include <cstdint>
#include <ostream>

#include "connection.hh"

class Cursor
{
  float initial_delay_ms_;

  bool initialized {};
  uint64_t next_frame_index_ts_ {};
  uint32_t next_frame_index_ {};

  static constexpr float ALPHA = 1 / 50.0;
  float quality_ = 1.0;
  float average_safety_margin_ {};

  unsigned int inc_plus {}, inc_minus {}, resets {};

  static bool has_frame( const NetworkEndpoint& endpoint, const uint32_t frame_index );

public:
  Cursor( const float initial_delay_ms )
    : initial_delay_ms_( initial_delay_ms )
  {}

  void sample( const NetworkEndpoint& endpoint, const uint64_t now );

  uint64_t initial_delay_ms() const { return initial_delay_ms_; }
  float quality() const { return quality_; }
  float safety_margin_ms() const { return average_safety_margin_; }
  uint32_t next_frame_index() const { return next_frame_index_; }

  void summary( std::ostream& out ) const;
};
