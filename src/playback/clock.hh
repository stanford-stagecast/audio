#pragma once

#include <cmath>
#include <cstdint>
#include <optional>
#include <ostream>
#include <utility>

class Clock
{
  /* When was the clock last updated? */
  uint64_t global_ts_last_update_;
  uint64_t local_ts_last_sample_;

  /* Dynamic state: current value of the clock */
  uint64_t local_clock_;

  static constexpr int64_t MAX_GAP = 24000; /* 500 ms @ 48 kHz */

  bool synced_;

public:
  Clock( const uint64_t global_ts );

  void time_passes( const uint64_t global_ts );

  void new_sample( const uint64_t global_ts, const uint64_t local_ts_sample );

  bool synced() const { return synced_; }

  std::optional<size_t> value() const
  {
    if ( synced_ ) {
      return local_clock_;
    } else {
      return std::nullopt;
    }
  }

  void summary( std::ostream& out ) const;
};
