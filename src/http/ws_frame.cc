#include "ws_frame.hh"

#include <array>

using namespace std;

void WebSocketFrame::parse( Parser& p )
{
  uint8_t octet {};

  /* first octet: fin, RSV1-3, opcode */
  p.integer( octet );

  fin = octet & 0b1000'0000;

  if ( octet & 0b0111'0000 ) {
    p.set_error();
    return;
  }

  opcode = opcode_t( octet & 0b0000'1111 );

  /* second octet: mask bit and payload_length sigil */
  p.integer( octet );

  const bool mask_bit = octet & 0b1000'0000;
  uint64_t payload_length = octet & 0b0111'1111;

  /* values 126 and 127 signal 2-byte and 4-byte payload length to follow */
  if ( payload_length == 126 ) {
    uint16_t len16 {};
    p.integer( len16 );
    if ( len16 < 126 ) {
      p.set_error();
      return;
    }
    payload_length = len16;
  } else if ( payload_length == 127 ) {
    p.integer( payload_length );
    if ( payload_length <= numeric_limits<uint16_t>::max() or payload_length > numeric_limits<int64_t>::max() ) {
      p.set_error();
      return;
    }
  }

  /* mask bit signals optional 4-byte mask */
  if ( mask_bit ) {
    masking_key.emplace();
    p.integer( masking_key.value() );
  } else {
    masking_key.reset();
  }

  /* check payload_length for reasonableness before allocating memory */
  if ( p.input().size() < payload_length ) {
    p.set_error();
    return;
  }

  payload.resize( payload_length );
  p.string( string_span::from_view( payload ) );

  /* remove masking */
  if ( masking_key.has_value() ) {
    array<uint8_t, 4> mk;
    memcpy( mk.data(), &masking_key.value(), sizeof( masking_key.value() ) );
    for ( size_t i = 0; i < payload.length(); i++ ) {
      payload[i] ^= mk[i % 4];
    }
  }
}

void WebSocketFrame::serialize( Serializer& s ) const
{
  /* first octet: fin, RSV1-3 all zero, opcode */
  s.integer( ( fin << 7 ) | uint8_t( opcode ) );

  /* next: mask bit and payload_length */
  const uint8_t mask_bit = masking_key.has_value() << 7;
  if ( payload.size() < 126 ) {
    s.integer( mask_bit | uint8_t( payload.size() ) );
  } else if ( payload.size() <= numeric_limits<uint16_t>::max() ) {
    s.integer( mask_bit | 126 );
    s.integer( uint16_t( payload.size() ) );
  } else if ( payload.size() <= numeric_limits<int64_t>::max() ) {
    s.integer( mask_bit | 127 );
    s.integer( uint64_t( payload.size() ) );
  } else {
    throw runtime_error( "invalid WebSocketFrame payload length" );
  }

  /* masking key */
  if ( masking_key.has_value() ) {
    s.integer( masking_key.value() );
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

uint32_t WebSocketFrame::serialized_length() const
{
  uint32_t ret = 2; /* first octet, mask bit, payload_length sigil */
  if ( payload.size() < 126 ) {
    /* do nothing */
  } else if ( payload.size() <= numeric_limits<uint16_t>::max() ) {
    ret += sizeof( uint16_t );
  } else if ( payload.size() <= numeric_limits<int64_t>::max() ) {
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
