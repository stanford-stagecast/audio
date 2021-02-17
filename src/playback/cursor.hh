#pragma once

#include "connection.hh"
#include "decoder_process.hh"

#include <rubberband/RubberBandStretcher.h>

class Cursor
{
  uint32_t target_lag_samples_; /* initialized lag, and end of time-compression */
  uint32_t max_lag_samples_;    /* if lag gets this big, start time-compression */
  bool compressing_ {};

  struct Statistics
  {
    unsigned int resets;
    int64_t min_margin_to_frontier;
    float mean_margin_to_frontier, mean_margin_to_safe_index, quality, mean_time_ratio;
  } stats_ {};

  size_t num_samples_output_ {};
  std::optional<uint64_t> frame_cursor_ {};

  uint64_t cursor_location() const { return frame_cursor_.value() * opus_frame::NUM_SAMPLES_MINLATENCY; }
  uint64_t greatest_read_location() const { return cursor_location() + opus_frame::NUM_SAMPLES_MINLATENCY - 1; }

  static constexpr float ALPHA = 0.01;

  void miss();
  void hit();

  RubberBand::RubberBandStretcher stretcher_;

public:
  Cursor( const uint32_t target_lag_samples, const uint32_t max_lag_samples );

  void sample( const PartialFrameStore& frames,
               const size_t global_sample_index,
               const size_t frontier_sample_index,
               const size_t safe_sample_index,
               OpusDecoderProcess& decoder,
               ChannelPair& output );

  void summary( std::ostream& out ) const;

  size_t ok_to_pop( const PartialFrameStore& frames ) const;

  void set_target_lag( const unsigned int num_samples ) { target_lag_samples_ = num_samples; }

  void reset() { frame_cursor_.reset(); }
};
