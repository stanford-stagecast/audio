#pragma once

#include "connection.hh"
#include "decoder_process.hh"
#include "opus.hh"

#include <rubberband/RubberBandStretcher.h>

class Cursor
{
  uint32_t target_lag_samples_; /* initialized lag, and end of time-compression */

  uint32_t min_lag_samples_; /* if lag gets this small, start time-expansion */
  uint32_t max_lag_samples_; /* if lag gets this big, start time-compression */

  enum class Rate : uint8_t
  {
    Steady,
    Compressing,
    Expanding
  } rate_ { Rate::Steady };

  struct Statistics
  {
    unsigned int resets;
    float mean_margin_to_frontier, quality, mean_time_ratio;
    unsigned int compress_starts, compress_stops;
    unsigned int expand_starts, expand_stops;
  } stats_ {};

  std::optional<size_t> num_samples_output_ {};
  std::optional<uint64_t> frame_cursor_ {};

  uint64_t cursor_location() const { return frame_cursor_.value() * opus_frame::NUM_SAMPLES_MINLATENCY; }
  uint64_t greatest_read_location() const { return cursor_location() + opus_frame::NUM_SAMPLES_MINLATENCY - 1; }

  static constexpr float ALPHA = 0.01;

  void miss();
  void hit();

public:
  Cursor( const uint32_t target_lag_samples, const uint32_t min_lag_samples, const uint32_t max_lag_samples );

  struct AudioSlice
  {
    std::array<float, 1024> ch1, ch2;
    size_t sample_index;
    uint16_t length;
    bool good;

    span_view<float> ch1_span() const { return { ch1.data(), length }; }
    span_view<float> ch2_span() const { return { ch2.data(), length }; }
  };

  void sample( const PartialFrameStore<AudioFrame>& frames,
               const size_t frontier_sample_index,
               OpusDecoderProcess& decoder,
               RubberBand::RubberBandStretcher& stretcher,
               AudioSlice& output );

  void setup( const size_t global_sample_index, const size_t frontier_sample_index );
  bool initialized() const { return frame_cursor_.has_value(); }
  void summary( std::ostream& out ) const;

  size_t ok_to_pop( const PartialFrameStore<AudioFrame>& frames ) const;

  void set_target_lag( const unsigned int target_samples,
                       const unsigned int min_samples,
                       const unsigned int max_samples )
  {
    target_lag_samples_ = target_samples;
    min_lag_samples_ = min_samples;
    max_lag_samples_ = max_samples;
  }

  size_t num_samples_output() const { return num_samples_output_.value(); }
};
