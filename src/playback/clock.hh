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

  static constexpr int64_t MAX_DIFFERENCE = 9600; /* 200 ms @ 48 kHz */
  static constexpr double CLOCK_SLEW_ALPHA = 0.01;
  static constexpr double CLOCK_SLEW_HORIZON = 480000; /* 10 s @ 48 kHz */

  void reset( const uint64_t global_ts, const uint64_t local_ts_initial_value );
  unsigned int resets_ { 0 };

  unsigned int gaps_ { 0 }, avulsions_ { 0 };

  double last_clock_difference_ {};

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
