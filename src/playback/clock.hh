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
  double local_clock_ = local_ts_last_sample_;

  /* Rate of the clock */
  double clock_rate_ { 1 };

  static constexpr int64_t MAX_GAP = 24000; /* 500 ms @ 48 kHz */
  static constexpr int64_t MAX_SKEW = 4800; /* 100 ms @ 48 kHz */
  static constexpr double ALPHA = 0.01;
  static constexpr double INTERCEPT_HORIZON = 48000 * 2;

  void reset( const uint64_t global_ts, const uint64_t local_ts_initial_value );

  struct Statistics
  {
    unsigned int resets {}, gaps {}, avulsions {};
    double last_clock_difference {};
    double smoothed_clock_difference {};
    uint64_t last_reset {};
    double biggest_gap_since_reset {}, biggest_diff_since_reset {};
    double jitter_squared;
  } stats_ {};

  bool synced_;

public:
  Clock( const uint64_t global_ts );

  void time_passes( const uint64_t global_ts );

  void new_sample( const uint64_t global_ts, const uint64_t local_ts_sample );

  bool synced() const { return synced_; }
  double rate() const { return clock_rate_; }
  float jitter() const { return sqrt( stats_.jitter_squared ); }
  std::optional<size_t> value() const
  {
    if ( synced_ ) {
      return lrint( local_clock_ );
    } else {
      return std::nullopt;
    }
  }

  void summary( std::ostream& out ) const;
};
