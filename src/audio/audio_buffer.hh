#pragma once

#include "typed_ring_buffer.hh"

using AudioChannel = SafeEndlessBuffer<float>;

class AudioBuffer
{
  AudioChannel ch1_, ch2_;

public:
  AudioBuffer( const size_t capacity )
    : ch1_( capacity )
    , ch2_( capacity )
  {}

  size_t range_begin() const { return ch1_.range_begin(); }
  size_t range_end() const { return ch1_.range_end(); }

  void pop( const size_t num_samples )
  {
    ch1_.pop( num_samples );
    ch2_.pop( num_samples );
  }

  std::pair<float, float> safe_get( const size_t index ) const
  {
    return { ch1_.safe_get( index ), ch2_.safe_get( index ) };
  }

  void safe_set( const size_t index, const std::pair<float, float> val )
  {
    ch1_.safe_set( index, val.first );
    ch2_.safe_set( index, val.second );
  }

  AudioChannel& ch1() { return ch1_; }
  AudioChannel& ch2() { return ch2_; }

  const AudioChannel& ch1() const { return ch1_; }
  const AudioChannel& ch2() const { return ch2_; }
};
