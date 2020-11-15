#pragma once

#include <cstdint>

#include "opus.hh"
#include "parser.hh"
#include "spans.hh"

struct AudioFrame
{
  uint32_t frame_index {}; // units of opus_frame::NUM_SAMPLES, about four months at 2^32 * 120 / 48 kHz
  opus_frame ch1 {}, ch2 {};

  size_t sample_index() const { return frame_index * opus_frame::NUM_SAMPLES; }

  uint8_t serialized_length() const;
  void serialize( Serializer& s ) const;
  void parse( Parser& p );
};

template<typename T>
struct NetInteger
{
  T val {};

  static constexpr uint8_t serialized_length() { return sizeof( T ); }
  void serialize( Serializer& s ) const { s.integer( val ); }
  void parse( Parser& p ) { p.integer( val ); }
};

template<typename T, uint8_t capacity_>
struct NetArray
{
  uint8_t length {};
  std::array<T, capacity_> elements {};

  static constexpr size_t capacity = capacity_;

  uint32_t serialized_length() const
  {
    if ( length > capacity ) {
      throw std::runtime_error( "invalid NetArray" );
    }

    uint32_t ret = sizeof( length );
    for ( uint8_t i = 0; i < length; i++ ) {
      ret += elements[i].serialized_length();
    }

    return ret;
  }

  void serialize( Serializer& s ) const
  {
    s.integer( length );
    for ( uint8_t i = 0; i < length; i++ ) {
      s.object( elements[i] );
    }
  }

  void parse( Parser& p )
  {
    p.integer( length );
    if ( length > capacity ) {
      p.set_error();
      return;
    }
    for ( uint8_t i = 0; i < length; i++ ) {
      p.object( elements[i] );
    }
  }
};

struct Packet
{
  uint32_t cumulative_ack {};
  NetArray<NetInteger<uint32_t>, 64> selective_acks {};
  NetArray<AudioFrame, 8> frames {};

  uint32_t serialized_length() const;
  void serialize( Serializer& s ) const;
  void parse( Parser& p );
};
