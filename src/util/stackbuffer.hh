#pragma once

#include <array>
#include <cstdint>

#include "parser.hh"
#include "spans.hh"

template<uint8_t alignment, typename size_type, size_type max_capacity>
class StackBuffer
{
  size_type length_ = 0;

  /* allow use of alignas(0) */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
  alignas( alignment ) std::array<char, max_capacity> buffer_;
#pragma GCC diagnostic pop

  string_span mutable_buffer_of_preset_length() { return { mutable_data_ptr(), length() }; }

public:
  /* allow use of buffer initialized */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
  StackBuffer() {}
#pragma GCC diagnostic pop

  const char* data_ptr() const { return buffer_.data(); }
  char* mutable_data_ptr() { return buffer_.data(); }

  const uint8_t* unsigned_data_ptr() const { return reinterpret_cast<const uint8_t*>( buffer_.data() ); }
  uint8_t* mutable_unsigned_data_ptr() { return reinterpret_cast<uint8_t*>( buffer_.data() ); }

  string_span mutable_buffer() { return { mutable_data_ptr(), max_capacity }; }

  std::string_view as_string_view() const { return { data_ptr(), length_ }; }
  operator std::string_view() const { return as_string_view(); }

  static constexpr size_type capacity() { return max_capacity; }
  size_type length() const { return length_; }

  void resize( const size_type new_length )
  {
    length_ = new_length;
    validate();
  }

  void validate() const
  {
    if ( length_ > max_capacity ) {
      throw std::runtime_error( "StackBuffer length is invalid: " + std::to_string( length_ ) + " > "
                                + std::to_string( max_capacity ) );
    }
  }

  size_type serialized_length() const { return sizeof( size_type ) + length_; }

  void serialize( Serializer& s ) const
  {
    s.integer( length_ );
    s.string( as_string_view() );
  }

  void parse( Parser& p )
  {
    p.integer( length_ );
    if ( length_ > max_capacity ) {
      p.set_error();
      return;
    }
    p.string( mutable_buffer_of_preset_length() );
  }
};
