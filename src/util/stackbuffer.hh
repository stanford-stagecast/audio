#pragma once

#include <array>
#include <cstdint>

#include "spans.hh"

template<uint8_t alignment, typename size_type, size_type max_capacity>
class StackBuffer
{
  size_type length_ = 0;

  alignas( alignment ) std::array<char, max_capacity> buffer_;

public:
  const char* data_ptr() const { return buffer_.data(); }
  char* mutable_data_ptr() { return buffer_.data(); }

  string_span mutable_buffer() { return { mutable_data_ptr(), max_capacity }; }

  std::string_view as_string_view() const { return { data_ptr(), length_ }; }
  operator std::string_view() const { return as_string_view(); }

  size_type capacity() const { return max_capacity; }
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

  /* allow use of buffer initialized */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
  StackBuffer() {}
#pragma GCC diagnostic pop
};
