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
    OpusEncoder enc_;
    std::optional<opus_frame> output_ {};
    size_t num_pushed_ {};

  public:
    TrackedEncoder( const int bit_rate, const int sample_rate );

    bool can_encode_frame( const size_t source_cursor ) const;
    void encode_one_frame( const AudioChannel& channel );
    size_t cursor() const { return num_pushed_ * opus_frame::NUM_SAMPLES_MINLATENCY; }

    std::optional<opus_frame>& output() { return output_; }
    const std::optional<opus_frame>& output() const { return output_; }

    void reset( const int bit_rate, const int sample_rate );
  };

  size_t num_popped_ {};

  AudioType audio_type_;

protected:
  TrackedEncoder enc1_;
  std::optional<TrackedEncoder> enc2_;

public:
  OpusEncoderProcess( const AudioType audio_type, const int bit_rate1, const int sample_rate );
  OpusEncoderProcess( const AudioType audio_type, const int bit_rate1, const int bit_rate2, const int sample_rate );

  bool has_frame() const;
  void pop_frame();

  void reset( const int bit_rate1, const int sample_rate );
  void reset( const int bit_rate1, const int bit_rate2, const int sample_rate );

  size_t min_encode_cursor() const;
  size_t frame_index() const { return num_popped_; }

  const opus_frame& front_enc1() const { return enc1_.output().value(); }
  const opus_frame& front_enc2() const { return enc2_.value().output().value(); }

  void encode_one_frame( const AudioChannel& ch1, const AudioChannel& ch2 );
  void encode_one_frame( const AudioChannel& frame );

  AudioType audio_type() const { return audio_type_; }
};

template<class AudioSource>
class EncoderTask : public OpusEncoderProcess
{
  std::shared_ptr<AudioSource> source_;

  void pop_from_source();

public:
  EncoderTask( const AudioType audio_type,
               const int bit_rate,
               const int sample_rate,
               const std::shared_ptr<AudioSource> source,
               EventLoop& loop );

  EncoderTask( const AudioType audio_type,
               const int bit_rate1,
               const int bit_rate2,
               const int sample_rate,
               const std::shared_ptr<AudioSource> source,
               EventLoop& loop );
};

using ClientEncoderTask = EncoderTask<AudioDeviceTask>;
