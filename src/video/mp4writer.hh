#pragma once

#include <memory>
#include <string>

extern "C"
{
#include <libavformat/avformat.h>
}

#include "exception.hh"
#include "file_descriptor.hh"
#include "opus.hh"
#include "ring_buffer.hh"
#include "socket.hh"

class MP4Writer
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

  AVStream* video_stream_;

  Address stream_destination_ { "127.0.0.1", 9016 };
  UDPSocket stream_socket_ {};

  static constexpr unsigned int BUF_SIZE = 1048576;
  RingBuffer buf_ { BUF_SIZE };

  constexpr static unsigned int MP4_TIMEBASE = 90000;

  bool header_written_;
  unsigned int frame_rate_, width_, height_;

  static int av_check( const int retval );

public:
  MP4Writer( const unsigned int frame_rate, const unsigned int width, const unsigned int height );

  ~MP4Writer();

  void write( span<uint8_t> nal, const int64_t pts, const int64_t dts );

  MP4Writer( const MP4Writer& other ) = delete;
  MP4Writer& operator=( const MP4Writer& other ) = delete;
};
