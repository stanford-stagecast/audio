#pragma once

#include <memory>
#include <opus/opus.h>

#include "stackbuffer.hh"

class opus_frame : public StackBuffer<0, uint8_t, 60>
{
public:
  static constexpr unsigned int NUM_SAMPLES_MINLATENCY = 120; /* 2.5 ms at 48 kHz */
};

static_assert( sizeof( opus_frame ) == 61 );

class OpusEncoder
{
  struct encoder_deleter
  {
    void operator()( OpusEncoder* x ) const;
  };

  std::unique_ptr<OpusEncoder, encoder_deleter> encoder_ {};
  uint8_t channels_;

public:
  OpusEncoder( const int bit_rate, const int sample_rate, const int channels, const int application );
  void encode( const span_view<float> samples, opus_frame& encoded_output );
  void encode_stereo( const span_view<float> ch1, const span_view<float> ch2, opus_frame& encoded_output );
};

class OpusDecoder
{
  struct decoder_deleter
  {
    void operator()( OpusDecoder* x ) const;
  };

  std::unique_ptr<OpusDecoder, decoder_deleter> decoder_;
  uint8_t channels_;

public:
  OpusDecoder( const int sample_rate, const int channels );
  void decode( const opus_frame& encoded_input, span<float> samples );
  void decode_stereo( const opus_frame& encoded_input, span<float> ch1, span<float> ch2 );
  void decode_missing( span<float> samples );
  void decode_missing_stereo( span<float> ch1, span<float> ch2 );
};
