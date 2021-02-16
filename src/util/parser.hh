#pragma once

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

#include "exception.hh"
#include "spans.hh"

class Parser
{
  bool error_ {};
  std::string_view input_;

  void check_size( const size_t size )
  {
    if ( size > input_.size() ) {
      set_error();
    }
  }

public:
  Parser( const std::string_view input )
    : input_( input )
  {}

  std::string_view input() const { return input_; }
  bool error() const { return error_; }
  void set_error() { error_ = true; }
  void clear_error() { error_ = false; }

  ~Parser()
  {
    if ( error_ ) {
      std::cerr << "Error: Parser destroyed without clearing error.\n";
      abort();
    }
  }

  template<typename T>
  void integer( T& out )
  {
    check_size( sizeof( T ) );
    if ( error() ) {
      return;
    }

    out = static_cast<T>( 0 );
    for ( size_t i = 0; i < sizeof( T ); i++ ) {
      out <<= 8;
      out += uint8_t( input_.at( i ) );
    }

    input_.remove_prefix( sizeof( T ) );
  }

  template<typename T>
  void floating( T& out )
  {
    check_size( sizeof( T ) );
    if ( error() ) {
      return;
    }

    memcpy( &out, input_.data(), sizeof( T ) );
    input_.remove_prefix( sizeof( T ) );
  }

  void string( string_span out )
  {
    check_size( out.size() );
    if ( error() ) {
      return;
    }
    memcpy( out.mutable_data(), input_.data(), out.size() );
    input_.remove_prefix( out.size() );
  }

  template<typename T>
  void object( T& out )
  {
    if ( not error() ) {
      out.parse( *this );
    }
  }
};

class Serializer
{
  string_span output_;
  size_t original_size_;

  void check_size( const size_t size )
  {
    if ( size > output_.size() ) {
      throw std::runtime_error( "no room to serialize" );
    }
  }

public:
  Serializer( string_span output )
    : output_( output )
    , original_size_( output.size() )
  {}

  size_t bytes_written() const { return original_size_ - output_.size(); }

  template<typename T>
  void integer( const T& val )
  {
    constexpr size_t len = sizeof( T );
    check_size( len );

    for ( size_t i = 0; i < len; ++i ) {
      *output_.mutable_data() = ( val >> ( ( len - i - 1 ) * 8 ) ) & 0xff;
      output_.remove_prefix( 1 );
    }
  }

  template<typename T>
  void floating( const T& val )
  {
    check_size( sizeof( T ) );
    memcpy( output_.mutable_data(), &val, sizeof( T ) );
    output_.remove_prefix( sizeof( T ) );
  }

  void string( const std::string_view str )
  {
    check_size( str.size() );
    memcpy( output_.mutable_data(), str.data(), str.size() );
    output_.remove_prefix( str.size() );
  }

  template<typename T>
  void object( const T& obj )
  {
    check_size( obj.serialized_length() );
    obj.serialize( *this );
  }
};
