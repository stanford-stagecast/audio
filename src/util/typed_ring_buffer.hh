#pragma once

#include "exception.hh"
#include "ring_buffer.hh"
#include "simple_string_span.hh"

template<typename T>
class simple_span
{
  simple_string_span storage_;

  static constexpr auto elem_size_ = sizeof( T );

public:
  simple_span( const char* addr, const size_t len )
    : storage_( addr, len * elem_size_ )
  {}

  simple_span( const simple_string_span s )
    : storage_( s )
  {
    if ( s.size() % elem_size_ ) {
      throw std::runtime_error( "invalid size " + std::to_string( s.size() ) );
    }
  }

  size_t size() const { return storage_.size() / elem_size_; }

  T* mutable_data() { return reinterpret_cast<T*>( storage_.mutable_data() ); }
  const T* data() const { return reinterpret_cast<T*>( storage_.data() ); }

  size_t copy( const simple_span<T> other ) { return storage_.copy( other.storage_ ) / elem_size_; }

  T& operator[]( const size_t N ) { return *( mutable_data() + N ); }
  const T& operator[]( const size_t N ) const { return *( mutable_data() + N ); }

  void remove_prefix( const size_t N ) { storage_.remove_prefix( N * elem_size_ ); }
  simple_span substr( const size_t pos, const size_t count )
  {
    return storage_.substr( pos * elem_size_, count * elem_size_ );
  }
};

template<typename T>
class const_simple_span : simple_span<T>
{
public:
  using simple_span<T>::simple_span;
  using simple_span<T>::size;
  using simple_span<T>::data;
  using simple_span<T>::operator[];
  using simple_span<T>::remove_prefix;
};

template<typename T>
class TypedRingBuffer
{
  RingBuffer storage_;

  static constexpr auto elem_size_ = sizeof( T );

public:
  explicit TypedRingBuffer( const size_t capacity )
    : storage_( capacity * elem_size_ )
  {}

  size_t capacity() const { return storage_.capacity() / elem_size_; }

  simple_span<T> writable_region() { return storage_.writable_region(); }
  const_simple_span<T> writable_region() const { return storage_.writable_region(); }
  void push( const size_t num_elems ) { storage_.push( num_elems * elem_size_ ); }

  const_simple_span<T> readable_region() const { return storage_.readable_region(); }
  void pop( const size_t num_elems ) { storage_.pop( num_elems * elem_size_ ); }

  size_t num_pushed() const { return storage_.bytes_pushed() / elem_size_; }
  size_t num_popped() const { return storage_.bytes_popped() / elem_size_; }
  size_t num_stored() const { return storage_.bytes_stored() / elem_size_; }
};

template<typename T>
class EndlessBuffer : TypedRingBuffer<T>
{

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
  EndlessBuffer( const size_t s_capacity )
    : TypedRingBuffer<T>( s_capacity )
  {
    auto writable = TypedRingBuffer<T>::writable_region();
    memset( writable.mutable_data(), 0, writable.size() );
  }

  using TypedRingBuffer<T>::TypedRingBuffer;
  using TypedRingBuffer<T>::capacity;
  using TypedRingBuffer<T>::num_pushed;
  using TypedRingBuffer<T>::num_popped;
  using TypedRingBuffer<T>::num_stored;

  using TypedRingBuffer<T>::push;
  using TypedRingBuffer<T>::pop;

  size_t range_begin() const { return num_popped(); }
  size_t range_end() const { return num_popped() + capacity(); }

  simple_span<T> writable_region( const size_t pos, const size_t count )
  {
    check_bounds( pos, count );
    return TypedRingBuffer<T>::writable_region().substr( pos - range_begin(), count );
  }

  const_simple_span<T> readable_region( const size_t pos, const size_t count ) const
  {
    check_bounds( pos, count );
    return TypedRingBuffer<T>::readable_region().substr( pos - range_begin(), count );
  }
};
