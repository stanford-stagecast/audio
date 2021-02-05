#pragma once

#include <memory>
#include <string>

#include "exception.hh"
#include "file_descriptor.hh"
#include "opus.hh"
#include "ring_buffer.hh"

extern "C"
{
#include <libavformat/avformat.h>
}

class WebMWriter
{
  struct avformat_deleter
  {
    void operator()( AVFormatContext* x ) const { avformat_free_context( x ); }
  };
  std::unique_ptr<AVFormatContext, avformat_deleter> context_ {};

  struct av_deleter
  {
    void operator()( uint8_t* x ) const { av_free( x ); }
  };
  std::unique_ptr<uint8_t, av_deleter> buffer_ {};

  AVStream* audio_stream_;

  static constexpr unsigned int BUF_SIZE = 65536;

  FileDescriptor init_, stream_;
  RingBuffer buf_ { BUF_SIZE };

  constexpr static unsigned int WEBM_TIMEBASE = 1000;

  bool header_written_;
  unsigned int sample_rate_;

  static int av_check( const int retval );

  void write_to_fd( FileDescriptor& target );

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
