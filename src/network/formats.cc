#include "formats.hh"
#include "exception.hh"
#include "opus.hh"

using namespace std;

uint8_t AudioMessage::serialized_length() const
{
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

void AudioMessage::serialize( const string_span s )
{
  if ( s.size() < serialized_length() ) {
    throw runtime_error( "no room to serialize AudioMessage: " + to_string( s.size() ) + " < "
                         + to_string( serialized_length() ) );
  }

  string_span out = s;

  write_value( out, frame_index );
  write_value( out, ch1.length() );
  write_string( out, ch1.as_string_view() );
  write_value( out, ch2.length() );
  write_string( out, ch2.as_string_view() );

  if ( out.size() + serialized_length() != s.size() ) {
    throw runtime_error( "AudioMessage::serialize internal error" );
  }
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

bool AudioMessage::parse( const string_view s )
{
  string_view in = s;

  read_value( in, frame_index );
  read_value( in, ch1.mutable_length() );
  if ( ch1.length() > opus_frame::MAX_LENGTH ) {
    return false;
  }
  read_string( in, ch1.as_string_span() );

  read_value( in, ch2.mutable_length() );
  if ( ch2.length() > opus_frame::MAX_LENGTH ) {
    return false;
  }
  read_string( in, ch2.as_string_span() );

  return true;
}
