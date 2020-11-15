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
    TypedRingBuffer<opus_frame> output_ { 64 };

  public:
    Channel( const int bit_rate, const int sample_rate );

    bool can_encode_frame( const size_t source_cursor ) const;
    void encode_one_frame( const AudioChannel& channel );
    size_t cursor() const;

    TypedRingBuffer<opus_frame>& output() { return output_; }
    const TypedRingBuffer<opus_frame>& output() const { return output_; }

    void reset( const int bit_rate, const int sample_rate );
  };

protected:
  Channel enc1_, enc2_;

public:
  OpusEncoderProcess( const int bit_rate, const int sample_rate );

  bool has_frame() const { return ( enc1_.output().num_stored() > 0 ) and ( enc2_.output().num_stored() > 0 ); }
  void pop_frame()
  {
    enc1_.output().pop( 1 );
    enc2_.output().pop( 1 );
  }

  void reset( const int bit_rate, const int sample_rate );

  size_t min_encode_cursor() const { return std::min( enc1_.cursor(), enc2_.cursor() ); }
  size_t frame_index() const { return enc1_.output().num_popped(); }

  const opus_frame& front_ch1() const { return enc1_.output().readable_region().at( 0 ); }
  const opus_frame& front_ch2() const { return enc2_.output().readable_region().at( 0 ); }
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
