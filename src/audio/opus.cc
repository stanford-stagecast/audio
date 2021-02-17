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

void OpusEncoder::encoder_deleter::operator()( OpusEncoder* x ) const
{
  opus_encoder_destroy( x );
}

OpusEncoder::OpusEncoder( const int bit_rate, const int sample_rate, const int channels, const int application )
  : channels_( channels )
{
  int out;

  /* create encoder */
  encoder_.reset(
    notnull( "opus_encoder_create", opus_encoder_create( sample_rate, channels, application, &out ) ) );
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
  if ( channels_ != 1 ) {
    throw runtime_error( "can't encode mono when channels != 1" );
  }

  if ( samples.size() != opus_frame::NUM_SAMPLES_MINLATENCY ) {
    throw runtime_error( "encode: wrong number of samples" );
  }

  encoded_output.resize( opus_check( opus_encode_float( encoder_.get(),
                                                        samples.data(),
                                                        samples.size(),
                                                        encoded_output.mutable_unsigned_data_ptr(),
                                                        encoded_output.capacity() ) ) );
}

void OpusEncoder::encode_stereo( const span_view<float> ch1,
                                 const span_view<float> ch2,
                                 opus_frame& encoded_output )
{
  if ( channels_ != 2 ) {
    throw runtime_error( "can't encode stereo when channels != 2" );
  }

  array<pair<float, float>, opus_frame::NUM_SAMPLES_MINLATENCY> interleave_buffer;

  if ( ch1.size() != opus_frame::NUM_SAMPLES_MINLATENCY or ch2.size() != opus_frame::NUM_SAMPLES_MINLATENCY ) {
    throw runtime_error( "encode_stereo: wrong number of samples" );
  }

  for ( unsigned int i = 0; i < opus_frame::NUM_SAMPLES_MINLATENCY; i++ ) {
    interleave_buffer[i] = { ch1[i], ch2[i] };
  }

  encoded_output.resize( opus_check( opus_encode_float( encoder_.get(),
                                                        &interleave_buffer[0].first,
                                                        opus_frame::NUM_SAMPLES_MINLATENCY,
                                                        encoded_output.mutable_unsigned_data_ptr(),
                                                        encoded_output.capacity() ) ) );
}

void OpusDecoder::decoder_deleter::operator()( OpusDecoder* x ) const
{
  opus_decoder_destroy( x );
}

OpusDecoder::OpusDecoder( const int sample_rate, const int channels )
  : decoder_()
  , channels_( channels )
{
  int out;
  decoder_.reset( notnull( "opus_decoder_create", opus_decoder_create( sample_rate, channels_, &out ) ) );
  opus_check( out );
}

void OpusDecoder::decode( const opus_frame& encoded_input, span<float> samples )
{
  if ( channels_ != 1 ) {
    throw runtime_error( "can't decode mono when channels != 1" );
  }

  if ( samples.size() != opus_frame::NUM_SAMPLES_MINLATENCY ) {
    throw runtime_error( "decode: wrong number of samples" );
  }

  const size_t samples_written = opus_check( opus_decode_float( decoder_.get(),
                                                                encoded_input.unsigned_data_ptr(),
                                                                encoded_input.length(),
                                                                samples.mutable_data(),
                                                                samples.size(),
                                                                0 ) );

  if ( samples_written != opus_frame::NUM_SAMPLES_MINLATENCY ) {
    throw runtime_error( "invalid count from opus_decode_float: " + to_string( samples_written ) );
  }
}

void OpusDecoder::decode_stereo( const opus_frame& encoded_input, span<float> ch1, span<float> ch2 )
{
  if ( channels_ != 2 ) {
    throw runtime_error( "can't decode stereo when channels != 2" );
  }

  array<pair<float, float>, opus_frame::NUM_SAMPLES_MINLATENCY> interleave_buffer;

  if ( ch1.size() != opus_frame::NUM_SAMPLES_MINLATENCY or ch2.size() != opus_frame::NUM_SAMPLES_MINLATENCY ) {
    throw runtime_error( "decode_stereo: wrong number of samples" );
  }

  const size_t samples_written = opus_check( opus_decode_float( decoder_.get(),
                                                                encoded_input.unsigned_data_ptr(),
                                                                encoded_input.length(),
                                                                &interleave_buffer[0].first,
                                                                interleave_buffer.size(),
                                                                0 ) );

  if ( samples_written != opus_frame::NUM_SAMPLES_MINLATENCY ) {
    throw runtime_error( "invalid count from opus_decode_float: " + to_string( samples_written ) );
  }

  for ( unsigned int i = 0; i < opus_frame::NUM_SAMPLES_MINLATENCY; i++ ) {
    tie( ch1[i], ch2[i] ) = interleave_buffer[i];
  }
}

void OpusDecoder::decode_missing( span<float> samples )
{
  if ( channels_ != 1 ) {
    throw runtime_error( "can't decode_missing mono when channels != 1" );
  }

  const size_t samples_written
    = opus_check( opus_decode_float( decoder_.get(), nullptr, 0, samples.mutable_data(), samples.size(), 0 ) );

  if ( samples_written != opus_frame::NUM_SAMPLES_MINLATENCY ) {
    throw runtime_error( "invalid count from opus_decode_float: " + to_string( samples_written ) );
  }
}

void OpusDecoder::decode_missing_stereo( span<float> ch1, span<float> ch2 )
{
  if ( channels_ != 2 ) {
    throw runtime_error( "can't decode_missing stereo when channels != 1" );
  }

  array<pair<float, float>, opus_frame::NUM_SAMPLES_MINLATENCY> interleave_buffer;

  if ( ch1.size() != opus_frame::NUM_SAMPLES_MINLATENCY or ch2.size() != opus_frame::NUM_SAMPLES_MINLATENCY ) {
    throw runtime_error( "decode_missing_stereo: wrong number of samples" );
  }

  const size_t samples_written = opus_check(
    opus_decode_float( decoder_.get(), nullptr, 0, &interleave_buffer[0].first, interleave_buffer.size(), 0 ) );

  if ( samples_written != opus_frame::NUM_SAMPLES_MINLATENCY ) {
    throw runtime_error( "invalid count from opus_decode_float: " + to_string( samples_written ) );
  }

  for ( unsigned int i = 0; i < opus_frame::NUM_SAMPLES_MINLATENCY; i++ ) {
    tie( ch1[i], ch2[i] ) = interleave_buffer[i];
  }
}
