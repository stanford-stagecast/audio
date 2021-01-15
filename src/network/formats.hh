#pragma once

#include <cstdint>

#include "base64.hh"
#include "crypto.hh"
#include "opus.hh"
#include "parser.hh"

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
  operator span_view<T>() const { return as_span_view(); }

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

template<uint8_t capacity>
class NetString
{
  std::string storage_;

public:
  uint32_t serialized_length() const
  {
    if ( storage_.size() > capacity ) {
      throw std::runtime_error( "invalid NetString" );
    }

    return 1 + storage_.size();
  }

  void serialize( Serializer& s ) const
  {
    s.integer( uint8_t( storage_.size() ) );
    s.string( storage_ );
  }

  void parse( Parser& p )
  {
    uint8_t length {};
    p.integer( length );
    if ( length > capacity ) {
      p.set_error();
      return;
    }
    storage_.resize( length );
    p.string( string_span::from_view( storage_ ) );
  }

  const std::string& str() const { return storage_; }
  operator const std::string &() const { return str(); }

  NetString( const std::string_view s )
    : storage_( s )
  {
    if ( storage_.size() > capacity ) {
      throw std::runtime_error( "invalid NetString" );
    }
  }
};

struct Packet
{
  struct Record
  {
    uint32_t sequence_number {};
    NetArray<NetInteger<uint32_t>, 8> frames {};
  };

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

struct KeyMessage
{
  static constexpr char keyreq_id = uint8_t( 254 );
  static constexpr char keyreq_server_id = uint8_t( 255 );

  NetInteger<uint8_t> id {};
  KeyPair key_pair {};

  constexpr uint32_t serialized_length() const { return id.serialized_length() + key_pair.serialized_length(); }
  void serialize( Serializer& s ) const;
  void parse( Parser& p );
};
