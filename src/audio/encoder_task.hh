#pragma once

#include "audio_task.hh"
#include "eventloop.hh"
#include "formats.hh"
#include "opus.hh"
#include "typed_ring_buffer.hh"

class OpusEncoderProcess
{
  class TrackedEncoder
  {
    int channel_count_;
    OpusEncoder enc_;
    std::optional<opus_frame> output_ {};
    size_t num_pushed_ {};

  public:
    TrackedEncoder( const int bit_rate, const int sample_rate, const int channel_count );

    bool can_encode_frame( const size_t source_cursor ) const;
    void encode_one_frame( const AudioChannel& channel );
    void encode_one_frame( const AudioChannel& ch1, const AudioChannel& ch2 );
    size_t cursor() const { return num_pushed_ * opus_frame::NUM_SAMPLES; }

    std::optional<opus_frame>& output() { return output_; }
    const std::optional<opus_frame>& output() const { return output_; }

    void reset( const int bit_rate, const int sample_rate );
  };

  size_t num_popped_ {};

protected:
  TrackedEncoder enc1_;
  std::optional<TrackedEncoder> enc2_;

public:
  OpusEncoderProcess( const int bit_rate, const int sample_rate );
  OpusEncoderProcess( const int bit_rate1, const int bit_rate2, const int sample_rate );

  bool has_frame() const;
  void pop_frame();

  void reset( const int bit_rate1, const int sample_rate );
  void reset( const int bit_rate1, const int bit_rate2, const int sample_rate );

  size_t min_encode_cursor() const;
  size_t frame_index() const { return num_popped_; }

  AudioFrame front( const uint32_t frame_index ) const;

  void encode_one_frame( const AudioChannel& ch1, const AudioChannel& ch2 );
};

template<class AudioSource>
class EncoderTask : public OpusEncoderProcess
{
  std::shared_ptr<AudioSource> source_;

  void pop_from_source();

public:
  EncoderTask( const int bit_rate1,
               const int bit_rate2,
               const int sample_rate,
               const std::shared_ptr<AudioSource> source,
               EventLoop& loop );
};

using ClientEncoderTask = EncoderTask<AudioDeviceTask>;
