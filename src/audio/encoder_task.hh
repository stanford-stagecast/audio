#pragma once

#include <queue>

#include "alsa_devices.hh"
#include "eventloop.hh"
#include "opus.hh"
#include "typed_ring_buffer.hh"

class OpusEncoderProcess
{
  OpusEncoder enc_;
  TypedRingBuffer<opus_frame> output_ { 4096 }; /* 4096 * 120 / 48000 = 10.24 seconds */

public:
  static constexpr unsigned int samples_per_frame = 120; /* 2.5 ms at 48 kHz */

  OpusEncoderProcess( const int bit_rate, const int sample_rate );

  bool can_encode_frame( const size_t source_cursor ) const;
  void encode_one_frame( const AudioChannel& channel );
  size_t cursor() const;

  TypedRingBuffer<opus_frame>& output() { return output_; }
  const TypedRingBuffer<opus_frame>& output() const { return output_; }

  void reset( const int bit_rate, const int sample_rate );
};

template<class AudioSource>
class OpusEncoderTask
{
  OpusEncoderProcess enc1_, enc2_;
  std::shared_ptr<AudioSource> source_;

  void pop_from_source();

public:
  OpusEncoderTask( const int bit_rate,
                   const int sample_rate,
                   const std::shared_ptr<AudioSource> source,
                   EventLoop& loop );

  bool has_frame() const { return ( enc1_.output().num_stored() > 0 ) and ( enc2_.output().num_stored() > 0 ); }
  void pop_frame()
  {
    enc1_.output().pop( 1 );
    enc2_.output().pop( 1 );
  }

  void reset( const int bit_rate, const int sample_rate );
};
