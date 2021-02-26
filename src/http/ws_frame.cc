#include "ws_frame.hh"

#include <array>
#include <limits>

using namespace std;

size_t WebSocketFrameReader::read( const string_view orig_input )
{
  if ( error_ or finished() ) {
    return 0;
  }

  string_view input = orig_input;

  if ( not bytes12_.finished() ) {
    input.remove_prefix( bytes12_.read( input ) );
    if ( bytes12_.finished() ) {
      process_bytes12();
    }
  } else if ( len16_reader_.has_value() and not len16_reader_->finished() ) {
    input.remove_prefix( len16_reader_->read( input ) );
    if ( len16_reader_->finished() ) {
      process_len16();
    }
  } else if ( len64_reader_.has_value() and not len64_reader_->finished() ) {
    input.remove_prefix( len64_reader_->read( input ) );
    if ( len64_reader_->finished() ) {
      process_len64();
    }
  } else if ( masking_key_reader_.has_value() and not masking_key_reader_->finished() ) {
    input.remove_prefix( masking_key_reader_->read( input ) );
    if ( masking_key_reader_->finished() ) {
      target_.masking_key.emplace( masking_key_reader_->value() );
    }
  } else if ( payload_reader_.has_value() and not payload_reader_->finished() ) {
    input.remove_prefix( payload_reader_->read( input ) );

    if ( payload_reader_->finished() and not finished() ) {
      complete();
    }
  } else if ( payload_reader_.has_value() and payload_reader_->finished() and not finished() ) {
    complete();
  }

  return orig_input.size() - input.size();
}

void WebSocketFrameReader::complete()
{
  target_.payload = payload_reader_->release();
  apply_mask();
  finished_ = true;
}

void WebSocketFrameReader::process_bytes12()
{
  /* first octet: fin, RSV1-3, opcode */
  const uint8_t b1 = bytes12_.value()[0];

  target_.fin = b1 & 0b1000'0000;

  if ( b1 & 0b0111'0000 ) {
    error_ = true;
    return;
  }

  target_.opcode = WebSocketFrame::opcode_t( b1 & 0b0000'1111 );
  if ( target_.opcode > WebSocketFrame::opcode_t::Pong ) {
    error_ = true;
    return;
  }

  /* second octet: mask bit and payload_length sigil */
  const uint8_t b2 = bytes12_.value()[1];
  if ( b2 & 0b1000'0000 ) {
    masking_key_reader_.emplace();
  }

  const uint8_t payload_length_sigil = b2 & 0b0111'1111;

  if ( payload_length_sigil < 126 ) {
    payload_reader_.emplace( move( target_.payload ), payload_length_sigil );
  } else if ( payload_length_sigil == 126 ) {
    len16_reader_.emplace();
  } else if ( payload_length_sigil == 127 ) {
    len64_reader_.emplace();
  }
}

void WebSocketFrameReader::process_len16()
{
  Parser p { len16_reader_->as_string_view() };
  uint16_t len16;
  p.integer( len16 );
  if ( p.error() or len16 < 126 ) {
    error_ = true;
    p.clear_error();
    return;
  }

  payload_reader_.emplace( move( target_.payload ), len16 );
}

void WebSocketFrameReader::process_len64()
{
  Parser p { len64_reader_->as_string_view() };
  uint64_t len64;
  p.integer( len64 );
  if ( p.error() or len64 <= numeric_limits<uint16_t>::max()
       or len64 > uint64_t( numeric_limits<int64_t>::max() ) ) {
    error_ = true;
    p.clear_error();
    return;
  }

  payload_reader_.emplace( move( target_.payload ), len64 );
}

void WebSocketFrameReader::apply_mask()
{
  if ( not target_.masking_key.has_value() ) {
    return;
  }

  /* remove masking */
  for ( size_t i = 0; i < target_.payload.length(); i++ ) {
    target_.payload[i] ^= ( *target_.masking_key )[i % 4];
  }
}

void WebSocketFrame::serialize( Serializer& s ) const
{
  /* first octet: fin, RSV1-3 all zero, opcode */
  s.integer( uint8_t( ( fin << 7 ) | uint8_t( opcode ) ) );

  /* next: mask bit and payload_length */
  const uint8_t mask_bit = masking_key.has_value() << 7;
  uint8_t b2;
  if ( payload.size() < 126 ) {
    b2 = mask_bit | payload.size();
    s.integer( b2 );
  } else if ( payload.size() <= numeric_limits<uint16_t>::max() ) {
    b2 = mask_bit | 126;
    s.integer( b2 );
    s.integer( uint16_t( payload.size() ) );
  } else if ( payload.size() <= uint64_t( numeric_limits<int64_t>::max() ) ) {
    b2 = mask_bit | 127;
    s.integer( b2 );
    s.integer( uint64_t( payload.size() ) );
  } else {
    throw runtime_error( "invalid WebSocketFrame payload length" );
  }

  /* masking key */
  if ( masking_key.has_value() ) {
    string_view array_sv { reinterpret_cast<const char*>( masking_key->data() ), masking_key->size() };
    s.string( array_sv );
  }

  /* serialize payload data (possibly masked) */
  if ( masking_key.has_value() ) {
    array<uint8_t, 4> mk;
    memcpy( mk.data(), &masking_key.value(), sizeof( masking_key.value() ) );
    for ( size_t i = 0; i < payload.length(); i++ ) {
      s.integer( uint8_t( payload[i] ^ mk[i % 4] ) );
    }
  } else {
    s.string( payload );
  }
}

void WebSocketFrame::serialize( string& out ) const
{
  out.resize( serialized_length() );
  Serializer s { string_span::from_view( out ) };
  serialize( s );
  if ( s.bytes_written() != out.size() ) {
    throw runtime_error( "WebSocketFrame serialization size mismatch" );
  }
}

uint32_t WebSocketFrame::serialized_length() const
{
  uint32_t ret = 2; /* first octet, mask bit, payload_length sigil */
  if ( payload.size() < 126 ) {
    /* do nothing */
  } else if ( payload.size() <= numeric_limits<uint16_t>::max() ) {
    ret += sizeof( uint16_t );
  } else if ( payload.size() <= uint64_t( numeric_limits<int64_t>::max() ) ) {
    ret += sizeof( uint64_t );
  } else {
    throw runtime_error( "invalid WebSocketFrame payload length" );
  }

  if ( masking_key.has_value() ) {
    ret += sizeof( masking_key.value() );
  }

  ret += payload.size();

  return ret;
}

void WebSocketFrame::clear()
{
  fin = {};
  opcode = {};
  masking_key.reset();
  payload.clear();
}
