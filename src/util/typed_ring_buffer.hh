#pragma once

#include "exception.hh"
#include "ring_buffer.hh"
#include "spans.hh"

#include <algorithm>

template<typename T>
class TypedRingStorage : public RingStorage
{
  static constexpr auto elem_size_ = sizeof( T );

protected:
  span<T> storage() { return span_view<T> { RingStorage::storage().data(), capacity() }; }
  span_view<T> storage() const { return { RingStorage::storage().data(), capacity() }; }

public:
  explicit TypedRingStorage( const size_t capacity )
    : RingStorage( capacity * elem_size_ )
  {}

  size_t capacity() const { return RingStorage::capacity() / elem_size_; }
};

template<typename T>
class EndlessBuffer : TypedRingStorage<T>
{
  size_t num_popped_ = 0;

  void check_bounds( const size_t pos, const size_t count ) const
  {
    if ( pos < range_begin() ) {
      throw std::out_of_range( std::to_string( pos ) + " < " + std::to_string( range_begin() ) );
    }

    if ( pos + count > range_end() ) {
      throw std::out_of_range( std::to_string( pos ) + " + " + std::to_string( count ) + " > "
                               + std::to_string( range_end() ) );
    }
  }

public:
  using TypedRingStorage<T>::TypedRingStorage;

  void pop( const size_t num_elems )
  {
    auto region_to_erase = region( range_begin(), num_elems );
    std::fill( region_to_erase.begin(), region_to_erase.end(), T {} );
    num_popped_ += num_elems;
  }

  size_t range_begin() const { return num_popped_; }
  size_t range_end() const { return range_begin() + TypedRingStorage<T>::capacity(); }

  span<T> region( const size_t pos, const size_t count )
  {
    check_bounds( pos, count );
    return TypedRingStorage<T>::storage().substr( pos - range_begin(), count );
  }

  span_view<T> region( const size_t pos, const size_t count ) const
  {
    check_bounds( pos, count );
    return TypedRingStorage<T>::storage().substr( pos - range_begin(), count );
  }
};
