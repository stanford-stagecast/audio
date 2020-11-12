#include "opus.hh"

#include <iostream>

using namespace std;

int opus_check( const int retval )
{
  if ( retval < 0 ) {
    throw std::runtime_error( "Opus error: " + std::string( opus_strerror( retval ) ) );
  }

  return retval;
}

void OpusEncoder::encoder_deleter::operator()( OpusEncoder* x ) const
{
  opus_encoder_destroy( x );
}

OpusEncoder::OpusEncoder( const int bit_rate, const int sample_rate )
{
  int out;

  /* create encoder */
  encoder_.reset( notnull( "opus_encoder_create",
                           opus_encoder_create( sample_rate, 1, OPUS_APPLICATION_RESTRICTED_LOWDELAY, &out ) ) );
  opus_check( out );

  /* set bit rate */
  opus_check( opus_encoder_ctl( encoder_.get(), OPUS_SET_BITRATE( bit_rate ) ) );

  /* check bitrate */
  opus_check( opus_encoder_ctl( encoder_.get(), OPUS_GET_BITRATE( &out ) ) );
  if ( out != bit_rate ) {
    throw runtime_error( "bit rate mismatch" );
  }

  /* check sample rate */
  opus_check( opus_encoder_ctl( encoder_.get(), OPUS_GET_SAMPLE_RATE( &out ) ) );
  if ( out != sample_rate ) {
    throw runtime_error( "sample rate mismatch" );
  }

  /* check lookahead */
  opus_check( opus_encoder_ctl( encoder_.get(), OPUS_GET_LOOKAHEAD( &out ) ) );
  cerr << "Opus lookahead: " << out << "\n";
}

size_t OpusEncoder::encode( const span_view<float> samples, string_span encoded_output )
{
  const size_t bytes_written
    = opus_check( opus_encode_float( encoder_.get(),
                                     samples.data(),
                                     samples.size(),
                                     reinterpret_cast<unsigned char*>( encoded_output.mutable_data() ),
                                     encoded_output.size() ) );
  if ( bytes_written > encoded_output.size() ) {
    throw runtime_error( "Opus wrote too much: " + to_string( bytes_written ) + " > "
                         + to_string( encoded_output.size() ) );
  }

  return bytes_written;
}

void OpusDecoder::decoder_deleter::operator()( OpusDecoder* x ) const
{
  opus_decoder_destroy( x );
}

OpusDecoder::OpusDecoder( const int sample_rate )
{
  int out;

  decoder_.reset( notnull( "opus_decoder_create", opus_decoder_create( sample_rate, 1, &out ) ) );
  opus_check( out );
}

size_t OpusDecoder::decode( const string_span encoded_input, span<float> samples )
{
  const size_t samples_written
    = opus_check( opus_decode_float( decoder_.get(),
                                     reinterpret_cast<const unsigned char*>( encoded_input.data() ),
                                     encoded_input.size(),
                                     samples.mutable_data(),
                                     samples.size(),
                                     0 ) );

  return samples_written;
}
