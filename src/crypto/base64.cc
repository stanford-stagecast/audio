/*
    Mosh: the mobile shell
    Copyright 2012 Keith Winstein

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    In addition, as a special exception, the copyright holders give
    permission to link the code of portions of this program with the
    OpenSSL library under certain conditions as described in each
    individual source file, and distribute linked combinations including
    the two.

    You must obey the GNU General Public License in all respects for all
    of the code used other than OpenSSL. If you modify file(s) with this
    exception, you may extend this exception to your version of the
    file(s), but you are not obligated to do so. If you do not wish to do
    so, delete this exception statement from your version. If you delete
    this exception statement from all source files in the program, then
    also delete it here.
*/

#include <cstdlib>
#include <cstring>

#include <unistd.h>

#include "base64.hh"
#include "exception.hh"

using namespace std;

static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const unsigned char reverse[] = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0x3e, 0xff, 0xff, 0xff, 0x3f, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c,
  0x3d, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
  0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a,
  0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

/* Reverse maps from an ASCII char to a base64 sixbit value.  Returns > 0x3f on failure. */
static unsigned char base64_char_to_sixbit( unsigned char c )
{
  return reverse[c];
}

bool base64_decode( const char* b64, const size_t b64_len, uint8_t* raw, const size_t raw_len )
{
  if ( b64_len != 24 or raw_len != 16 ) {
    throw runtime_error( "base64_decode: unexpected length" );
  }

  uint32_t bytes = 0;
  for ( int i = 0; i < 22; i++ ) {
    unsigned char sixbit = base64_char_to_sixbit( *( b64++ ) );
    if ( sixbit > 0x3f ) {
      return false;
    }
    bytes <<= 6;
    bytes |= sixbit;
    /* write groups of 3 */
    if ( i % 4 == 3 ) {
      raw[0] = bytes >> 16;
      raw[1] = bytes >> 8;
      raw[2] = bytes;
      raw += 3;
      bytes = 0;
    }
  }
  /* last byte of output */
  *raw = bytes >> 4;
  if ( b64[0] != '=' || b64[1] != '=' ) {
    return false;
  }
  return true;
}

void base64_encode( const uint8_t* raw, const size_t raw_len, char* b64, const size_t b64_len )
{
  if ( b64_len != 24 or raw_len != 16 ) {
    throw runtime_error( "base64_encode: unexpected length" );
  }

  /* first 15 bytes of input */
  for ( int i = 0; i < 5; i++ ) {
    uint32_t bytes = ( raw[0] << 16 ) | ( raw[1] << 8 ) | raw[2];
    b64[0] = table[( bytes >> 18 ) & 0x3f];
    b64[1] = table[( bytes >> 12 ) & 0x3f];
    b64[2] = table[( bytes >> 6 ) & 0x3f];
    b64[3] = table[(bytes)&0x3f];
    raw += 3;
    b64 += 4;
  }

  /* last byte of input, last 4 of output */
  uint8_t lastchar = *raw;
  b64[0] = table[( lastchar >> 2 ) & 0x3f];
  b64[1] = table[( lastchar << 4 ) & 0x3f];
  b64[2] = '=';
  b64[3] = '=';
}

Base64Key::Base64Key()
  : key_()
{
  /* make random key */
  CheckSystemCall( "getentropy", getentropy( key_.data(), key_.size() ) );
}

Base64Key::PrintableKey Base64Key::printable_key() const
{
  array<char, 24> key_src;
  base64_encode( key_.data(), key_.size(), key_src.data(), key_src.size() );

  PrintableKey ret;
  memcpy( ret.printable_key.data(), key_src.data(), ret.printable_key.size() );
  return ret;
}

Base64Key::Base64Key( const string_view printable_key )
  : key_()
{
  if ( printable_key.size() != sizeof( PrintableKey::printable_key ) ) {
    throw runtime_error( "Base64Key must be 22 letters long" );
  }

  array<char, 24> key_dst;
  memcpy( key_dst.data(), printable_key.data(), printable_key.size() );
  key_dst[22] = key_dst[23] = '=';

  if ( not base64_decode( key_dst.data(), key_dst.size(), key_.data(), key_.size() ) ) {
    throw runtime_error( "Base64Key was not well-formed." );
  }

  if ( printable_key != this->printable_key() ) {
    throw runtime_error( "Base64Key was not encoded from a 128-bit key" );
  }
}
