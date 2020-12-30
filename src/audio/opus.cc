#include <iostream>
#include <opus/opus.h>

#include "opus.hh"

using namespace std;

int opus_check( const int retval )
{
  if ( retval < 0 ) {
    throw std::runtime_error( "Opus error: " + std::string( opus_strerror( retval ) ) );
  }

  return retval;
}

void opus_frame::resize( const uint8_t new_length )
{
  if ( new_length > MAX_LENGTH ) {
    throw std::out_of_range( "opus_frame::resize: " + std::to_string( new_length ) + " > "
                             + std::to_string( MAX_LENGTH ) );
  }

  length_ = new_length;
}

void opus_frame::parse( Parser& p )
{
  p.integer( length_ );
  if ( length_ > MAX_LENGTH ) {
    p.set_error();
    return;
  }
  p.string( as_string_span() );
}

void opus_frame::serialize( Serializer& s ) const
{
  s.integer( length_ );
  s.string( as_string_span() );
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
  if ( out != 120 ) {
    throw runtime_error( "unexpected Opus lookahead value:" + to_string( out ) );
  }
}

void OpusEncoder::encode( const span_view<float> samples, opus_frame& encoded_output )
{
  encoded_output.resize( opus_check( opus_encode_float(
    encoder_.get(), samples.data(), samples.size(), encoded_output.data(), opus_frame::MAX_LENGTH ) ) );
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

void OpusDecoder::decode( const opus_frame& encoded_input, span<float> samples )
{
  const size_t samples_written = opus_check( opus_decode_float(
    decoder_.get(), encoded_input.data(), encoded_input.length(), samples.mutable_data(), samples.size(), 0 ) );

  if ( samples_written != opus_frame::NUM_SAMPLES ) {
    throw runtime_error( "invalid count from opus_decode_float: " + to_string( samples_written ) );
  }
}

void OpusDecoder::decode_missing( span<float> samples )
{
  const size_t samples_written
    = opus_check( opus_decode_float( decoder_.get(), nullptr, 0, samples.mutable_data(), samples.size(), 0 ) );

  if ( samples_written != opus_frame::NUM_SAMPLES ) {
    throw runtime_error( "invalid count from opus_decode_float: " + to_string( samples_written ) );
  }
}
