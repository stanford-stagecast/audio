#pragma once

#include <cstdint>
#include <ostream>

#include "connection.hh"

class Cursor
{
  bool adjust_delay_;
  float initial_delay_ms_;

  bool initialized {};
  uint64_t next_frame_index_ts_ {};
  uint32_t next_frame_index_ {};

  uint64_t next_adjustment_ts_ {};

  static constexpr float ALPHA = 1 / 50.0;
  static constexpr float SLOW_ALPHA = 1 / 1000.0;
  float quality_ = 1.0;
  float average_safety_margin_ {};
  float average_safety_margin_slow_ {};

  unsigned int inc_plus {}, inc_minus {}, resets {};

  static bool has_frame( const NetworkEndpoint& endpoint, const uint32_t frame_index );

public:
  Cursor( const float initial_delay_ms, const bool adjust_delay )
    : adjust_delay_( adjust_delay )
    , initial_delay_ms_( initial_delay_ms )
  {}

  void sample( const NetworkEndpoint& endpoint, const uint64_t now );

  uint64_t initial_delay_ms() const { return initial_delay_ms_; }
  float quality() const { return quality_; }
  float safety_margin_ms() const { return average_safety_margin_; }
  uint32_t next_frame_index() const { return next_frame_index_; }

  void summary( std::ostream& out ) const;
};
