#pragma once

#include "audio_task.hh"
#include "eventloop.hh"
#include "opus.hh"
#include "typed_ring_buffer.hh"

class OpusEncoderProcess
{
  class Channel
  {
    OpusEncoder enc_;
    std::optional<opus_frame> output_ {};
    size_t num_pushed_ {};

  public:
    Channel( const int bit_rate, const int sample_rate );

    bool can_encode_frame( const size_t source_cursor ) const;
    void encode_one_frame( const AudioChannel& channel );
    size_t cursor() const { return num_pushed_ * opus_frame::NUM_SAMPLES; }

    std::optional<opus_frame>& output() { return output_; }
    const std::optional<opus_frame>& output() const { return output_; }

    void reset( const int bit_rate, const int sample_rate );
  };

  size_t num_popped_ {};

protected:
  Channel enc1_, enc2_;

public:
  OpusEncoderProcess( const int bit_rate, const int sample_rate );

  bool has_frame() const { return enc1_.output().has_value() and enc2_.output().has_value(); }
  void pop_frame()
  {
    if ( not has_frame() ) {
      throw std::runtime_error( "pop_frame() but not has_frame()" );
    }
    enc1_.output().reset();
    enc2_.output().reset();
    num_popped_++;
  }

  void reset( const int bit_rate, const int sample_rate );

  size_t min_encode_cursor() const { return std::min( enc1_.cursor(), enc2_.cursor() ); }
  size_t frame_index() const { return num_popped_; }

  const opus_frame& front_ch1() const { return enc1_.output().value(); }
  const opus_frame& front_ch2() const { return enc2_.output().value(); }

  void encode_one_frame( const AudioChannel& ch1, const AudioChannel& ch2 );
};

template<class AudioSource>
class EncoderTask : public OpusEncoderProcess
{
  std::shared_ptr<AudioSource> source_;

  void pop_from_source();

public:
  EncoderTask( const int bit_rate,
               const int sample_rate,
               const std::shared_ptr<AudioSource> source,
               EventLoop& loop );
};

using ClientEncoderTask = EncoderTask<AudioDeviceTask>;
