#pragma once

#include <memory>
#include <opus/opus.h>

#include "exception.hh"
#include "spans.hh"

class OpusEncoder
{
  struct encoder_deleter
  {
    void operator()( OpusEncoder* x ) const;
  };

  std::unique_ptr<OpusEncoder, encoder_deleter> encoder_ {};

public:
  OpusEncoder( const int bit_rate, const int sample_rate );
  size_t encode( const span_view<float> samples, string_span encoded_output );
};

class OpusDecoder
{
  struct decoder_deleter
  {
    void operator()( OpusDecoder* x ) const;
  };

  std::unique_ptr<OpusDecoder, decoder_deleter> decoder_ {};

public:
  OpusDecoder( const int sample_rate );
  size_t decode( const string_span encoded_input, span<float> samples );
};
