#pragma once

#include <cstdint>
#include <ostream>
#include <utility>

class Clock
{
  /* When was the clock last updated? */
  uint64_t global_ts_last_update_;
  uint64_t local_ts_last_sample_;

  /* Dynamic state: current value of the clock */
  double local_clock_ = local_ts_last_sample_;

  /* Semi-static state: offset and rate of the clock */
  int64_t offset_ = global_ts_last_update_ - local_ts_last_sample_;
  double clock_rate_ { 1 };

  static constexpr int64_t MAX_DIFFERENCE = 9600;
  static constexpr double CLOCK_SLEW_ALPHA = 0.001;

  void reset( const uint64_t global_ts, const uint64_t local_ts_initial_value );

  struct Statistics
  {
    unsigned int resets {}, gaps {}, avulsions {};
    double last_clock_difference {};
    double smoothed_clock_difference {};
    uint64_t last_reset {};
    double biggest_gap_since_reset {}, biggest_diff_since_reset {};
  } stats_ {};

  bool synced_;

public:
  Clock( const uint64_t global_ts );

  void time_passes( const uint64_t global_ts );

  void new_sample( const uint64_t global_ts, const uint64_t local_ts_sample );

  bool synced() const { return synced_; }
  int64_t offset() const { return offset_; }
  double rate() const { return clock_rate_; }

  void summary( std::ostream& out ) const;
};
