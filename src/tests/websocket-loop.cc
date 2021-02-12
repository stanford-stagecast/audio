#include <cstdlib>
#include <random>

#include "ws_frame.hh"

using namespace std;

class RNG
{
  default_random_engine gen_ { random_device {}() };
  bernoulli_distribution bit_ {};
  uniform_int_distribution<uint8_t> byte_ {};

public:
  bool bit() { return bit_( gen_ ); }
  uint8_t byte() { return byte_( gen_ ); }
};

WebSocketFrame random_frame( RNG& s, const unsigned int length )
{
  WebSocketFrame ret;
  ret.fin = s.bit();
  ret.opcode = WebSocketFrame::opcode_t( s.byte() & 0xF );
  ret.payload.resize( length );
  for ( unsigned int i = 0; i < length; i++ ) {
    ret.payload[i] = s.byte();
  }

  if ( s.bit() ) {
    ret.masking_key.emplace();
    ret.masking_key->at( 0 ) = s.byte();
    ret.masking_key->at( 1 ) = s.byte();
    ret.masking_key->at( 2 ) = s.byte();
    ret.masking_key->at( 3 ) = s.byte();
  }

  return ret;
}

void rt_verify( RNG& s, const unsigned int length )
{
  const WebSocketFrame f1 = random_frame( s, length );
  string f1_serialized;
  f1_serialized.resize( f1.serialized_length() );

  {
    Serializer ser { string_span::from_view( f1_serialized ) };
    ser.object( f1 );

    if ( ser.bytes_written() != f1_serialized.length() ) {
      throw runtime_error( "serialization length mismatch" );
    }
  }

  string_view input { f1_serialized };

  WebSocketFrame f2;
  {
    const bool byte_by_byte = s.bit();
    WebSocketFrameReader r { move( f2 ) };
    while ( not r.finished() ) {
      const string_view iview = byte_by_byte ? input.substr( 0, 1 ) : input.substr( 0, 4000 );
      input.remove_prefix( r.read( iview ) );
      if ( r.error() ) {
        throw runtime_error( "reader error" );
      }
    }
    f2 = r.release();
  }

  if ( f1 != f2 ) {
    abort();
    throw runtime_error( "mismatch with length " + to_string( length ) );
  }
}

void program_body()
{
  RNG rng;
  for ( unsigned int i = 0; i < 8; i++ ) {
    rt_verify( rng, 0 );
    rt_verify( rng, 1 );
    rt_verify( rng, 2 );
    rt_verify( rng, 3 );
    rt_verify( rng, 123 );
    rt_verify( rng, 124 );
    rt_verify( rng, 125 );
    rt_verify( rng, 126 );
    rt_verify( rng, 127 );
    rt_verify( rng, 128 );
    rt_verify( rng, 129 );
    rt_verify( rng, 130 );
    rt_verify( rng, 253 );
    rt_verify( rng, 254 );
    rt_verify( rng, 255 );
    rt_verify( rng, 256 );
    rt_verify( rng, 257 );
    rt_verify( rng, 258 );
    rt_verify( rng, 65533 );
    rt_verify( rng, 65534 );
    rt_verify( rng, 65535 );
    rt_verify( rng, 65536 );
    rt_verify( rng, 65537 );
    rt_verify( rng, 65538 );
    rt_verify( rng, 65539 );
    rt_verify( rng, 1000000 );
    rt_verify( rng, 1000000 + rng.byte() );
  }
}

int main()
{
  try {
    program_body();
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
