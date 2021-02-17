#pragma once

#include "connection.hh"
#include "decoder_process.hh"

#include <rubberband/RubberBandStretcher.h>

class Cursor
{
  uint32_t target_lag_samples_;

  struct Statistics
  {
    unsigned int resets;
    float mean_margin_to_frontier, mean_margin_to_safe_index, quality;
  } stats_ {};

  size_t num_samples_output_ {};
  std::optional<int64_t> cursor_location_ {};

  static constexpr float ALPHA = 0.01;

  void miss();
  void hit();

public:
  Cursor( const uint32_t target_lag_samples )
    : target_lag_samples_( target_lag_samples )
  {}

  void sample( const PartialFrameStore& frames,
               const size_t global_sample_index,
               const size_t frontier_sample_index,
               const size_t safe_sample_index,
               OpusDecoderProcess& decoder,
               ChannelPair& output );

  void summary( std::ostream& out ) const;

  size_t ok_to_pop( const PartialFrameStore& frames ) const;

  void set_target_lag( const unsigned int num_samples ) { target_lag_samples_ = num_samples; }

  void reset() { cursor_location_.reset(); }
};
