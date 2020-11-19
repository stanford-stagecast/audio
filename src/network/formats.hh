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

static_assert( sizeof( AudioFrame ) == 128 );

template<typename T>
struct NetInteger
{
  T value {};

  static constexpr uint8_t serialized_length() { return sizeof( T ); }
  void serialize( Serializer& s ) const { s.integer( value ); }
  void parse( Parser& p ) { p.integer( value ); }

  NetInteger() {}
  NetInteger( const T other )
    : value( other )
  {}
  operator T() const { return value; }
  NetInteger& operator=( const T other )
  {
    value = other;
    return *this;
  }
};

template<typename T, uint8_t capacity_>
struct NetArray
{
  std::array<T, capacity_> elements {};
  uint8_t length {};

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

  span_view<T> as_span_view() const { return span_view<T> { elements.data(), length }; }

  const T* begin() const { return as_span_view().begin(); }
  const T* end() const { return as_span_view().end(); }

  void push_back( const T& element )
  {
    if ( length >= capacity ) {
      throw std::out_of_range( "no room for NetArray::push_back" );
    }

    elements[length] = element;
    length++;
  }
};

struct Packet
{
  struct Record
  {
    uint32_t sequence_number {};
    NetArray<NetInteger<uint32_t>, 8> frames {};
  };

  uint8_t node_id {};

  struct SenderSection
  {
    uint32_t sequence_number {};
    NetArray<AudioFrame, 8> frames {};

    Record to_record() const;
  } sender_section {};

  struct ReceiverSection
  {
    uint32_t next_frame_needed {};
    NetArray<NetInteger<uint32_t>, 32> packets_received {};
  } receiver_section {};

  uint32_t serialized_length() const;
  void serialize( Serializer& s ) const;
  void parse( Parser& p );

  Packet() {}
  Packet( Parser& p ) { parse( p ); }
};
