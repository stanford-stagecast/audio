#pragma once

#include <memory>
#include <string>

#include "exception.hh"
#include "opus.hh"

extern "C"
{
#include <libavformat/avformat.h>
}

class WebMWriter
{
  struct av_deleter
  {
    void operator()( AVFormatContext* x ) const { avformat_free_context( x ); }
  };
  std::unique_ptr<AVFormatContext, av_deleter> context_ {};

  AVStream* audio_stream_;

  constexpr static unsigned int WEBM_TIMEBASE = 1000;

  bool header_written_;
  unsigned int sample_rate_;

  static int av_check( const int retval );

public:
  WebMWriter( const std::string& output_filename,
              const int bit_rate,
              const uint32_t sample_rate,
              const uint8_t num_channels );

  ~WebMWriter();

  void write( opus_frame& frame, const unsigned int starting_sample_number );

  WebMWriter( const WebMWriter& other ) = delete;
  WebMWriter& operator=( const WebMWriter& other ) = delete;
};
