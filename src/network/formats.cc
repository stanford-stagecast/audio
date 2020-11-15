#include "formats.hh"
#include "exception.hh"
#include "opus.hh"

using namespace std;

uint8_t AudioFrame::serialized_length() const
{
  if ( ch1.length() > opus_frame::MAX_LENGTH or ch2.length() > opus_frame::MAX_LENGTH ) {
    throw runtime_error( "invalid AudioFrame" );
  }

  return sizeof( frame_index ) + 2 + ch1.length() + ch2.length();
}

template<typename T>
void write_value( string_span& s, const T& val )
{
  if ( s.size() < sizeof( T ) ) {
    throw runtime_error( "no room to write value" );
  }

  memcpy( s.mutable_data(), &val, sizeof( T ) );
  s.remove_prefix( sizeof( T ) );
}

void write_string( string_span& s, const string_view str )
{
  if ( s.size() < str.size() ) {
    throw runtime_error( "no room to write string" );
  }

  memcpy( s.mutable_data(), str.data(), str.size() );
  s.remove_prefix( str.size() );
}

uint8_t AudioFrame::serialize( const string_span s ) const
{
  const auto len = serialized_length();
  if ( s.size() < len ) {
    throw runtime_error( "no room to serialize AudioFrame: " + to_string( s.size() ) + " < "
                         + to_string( serialized_length() ) );
  }

  string_span out = s;

  write_value( out, frame_index );
  write_value( out, ch1.length() );
  write_string( out, ch1.as_string_view() );
  write_value( out, ch2.length() );
  write_string( out, ch2.as_string_view() );

  if ( out.size() + len != s.size() ) {
    throw runtime_error( "AudioFrame::serialize internal error" );
  }

  return len;
}

template<typename T>
void read_value( string_view& s, T& out )
{
  if ( s.size() < sizeof( T ) ) {
    throw runtime_error( "no room to read value" );
  }

  memcpy( &out, s.data(), sizeof( T ) );
  s.remove_prefix( sizeof( T ) );
}

void read_string( string_view& s, string_span out )
{
  if ( s.size() < out.size() ) {
    throw runtime_error( "no room to read string" );
  }

  memcpy( out.mutable_data(), s.data(), out.size() );
  s.remove_prefix( out.size() );
}

uint8_t AudioFrame::parse( const string_view s )
{
  string_view in = s;

  read_value( in, frame_index );
  read_value( in, ch1.mutable_length() );
  if ( ch1.length() > opus_frame::MAX_LENGTH ) {
    return 0;
  }
  read_string( in, ch1.as_string_span() );

  read_value( in, ch2.mutable_length() );
  if ( ch2.length() > opus_frame::MAX_LENGTH ) {
    return 0;
  }
  read_string( in, ch2.as_string_span() );

  const auto len = serialized_length();
  if ( in.size() + len != s.size() ) {
    throw runtime_error( "AudioFrame::parse internal error" );
  }

  return len;
}

uint16_t Packet::serialized_length() const
{
  if ( num_frames > frames.size() ) {
    throw runtime_error( "invalid Packet" );
  }

  uint16_t ret = sizeof( num_frames );
  for ( uint8_t i = 0; i < num_frames; i++ ) {
    ret += frames.at( i ).serialized_length();
  }

  return ret;
}

uint16_t Packet::serialize( const string_span s ) const
{
  const auto len = serialized_length();
  if ( s.size() < len ) {
    throw runtime_error( "no room to serialize Packet: " + to_string( s.size() ) + " < "
                         + to_string( serialized_length() ) );
  }

  string_span out = s;

  write_value( out, num_frames );
  for ( uint8_t i = 0; i < num_frames; i++ ) {
    out.remove_prefix( frames.at( i ).serialize( out ) );
  }

  if ( out.size() + len != s.size() ) {
    throw runtime_error( "Packet::serialize internal error" );
  }

  return len;
}

uint16_t Packet::parse( const string_view s )
{
  string_view in = s;

  read_value( in, num_frames );
  if ( num_frames > frames.size() ) {
    return 0;
  }
  for ( uint8_t i = 0; i < num_frames; i++ ) {
    const auto len = frames.at( i ).parse( in );
    if ( not len ) {
      return 0;
    }
    in.remove_prefix( len );
  }

  const auto len = serialized_length();
  if ( in.size() + len != s.size() ) {
    throw runtime_error( "Packet::parse internal error" );
  }

  return len;
}
