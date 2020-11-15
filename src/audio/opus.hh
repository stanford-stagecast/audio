#pragma once

#include <memory>
#include <opus/opus.h>

#include "exception.hh"
#include "parser.hh"
#include "spans.hh"

class opus_frame
{
public:
  static constexpr unsigned int NUM_SAMPLES = 120; /* 2.5 ms at 48 kHz */
  static constexpr uint8_t MAX_LENGTH = 63;

private:
  std::array<char, MAX_LENGTH> storage_ {};
  uint8_t length_ = 0;

public:
  std::string_view as_string_view() const { return { storage_.data(), length_ }; }
  string_span as_string_span() const { return { storage_.data(), length_ }; }

  const uint8_t* data() const { return reinterpret_cast<const uint8_t*>( storage_.data() ); }
  uint8_t* data() { return reinterpret_cast<uint8_t*>( storage_.data() ); }

  uint8_t length() const { return length_; }

  void resize( const uint8_t new_length );

  uint8_t serialized_length() const { return 1 + length_; }
  void serialize( Serializer& s ) const;
  void parse( Parser& p );
};

static_assert( sizeof( opus_frame ) == 64 );

class OpusEncoder
{
  struct encoder_deleter
  {
    void operator()( OpusEncoder* x ) const;
  };

  std::unique_ptr<OpusEncoder, encoder_deleter> encoder_ {};

public:
  OpusEncoder( const int bit_rate, const int sample_rate );
  void encode( const span_view<float> samples, opus_frame& encoded_output );
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
  size_t decode( const opus_frame& encoded_input, span<float> samples );
};
