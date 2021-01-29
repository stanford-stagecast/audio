#pragma once

#include "connection.hh"
#include "decoder_process.hh"

class Cursor
{
  uint32_t target_lag_samples_;

  struct Statistics
  {
    unsigned int samples_inserted, samples_skipped, resets;
    int64_t last_skew;
  } stats_ {};

  OpusDecoderProcess decoder_ {};

  size_t num_samples_output_ {};
  std::optional<size_t> cursor_location_ {};

  static constexpr float ALPHA = 0.01;

  float quality_ = 1.0;
  void update_quality( const bool x ) { quality_ = ALPHA * x + ( 1 - ALPHA ) * quality_; }
  void miss() { update_quality( false ); };
  void hit() { update_quality( true ); }

  enum class Slew : uint8_t
  {
    NO,
    CONSUME_FASTER,
    CONSUME_SLOWER
  } slew_ { Slew::NO };

public:
  Cursor( const uint32_t target_lag_samples )
    : target_lag_samples_( target_lag_samples )
  {}

  void sample( const PartialFrameStore& frames,
               const size_t global_sample_index,
               const std::optional<size_t> local_clock_sample_index,
               AudioBuffer& output );

  void summary( std::ostream& out ) const;

  size_t ok_to_pop( const PartialFrameStore& frames ) const;

  void set_target_lag( const unsigned int num_samples ) { target_lag_samples_ = num_samples; }
};
