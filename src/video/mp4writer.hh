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

  uint8_t* buffer_ {};

  std::string extradata_ {};
  AVStream* video_stream_;
  bool idr_hit_ {};

  static constexpr unsigned int BUF_SIZE = 1048576;
  RingBuffer buf_ { BUF_SIZE };

  constexpr static unsigned int MP4_TIMEBASE = 90000;

  bool header_written_;
  unsigned int frame_rate_, width_, height_;

public:
  MP4Writer( const unsigned int frame_rate, const unsigned int width, const unsigned int height );

  ~MP4Writer();

  void write( const std::string_view nal, const uint32_t presentation_no, const uint32_t display_no );

  RingBuffer& output() { return buf_; }

  MP4Writer( const MP4Writer& other ) = delete;
  MP4Writer& operator=( const MP4Writer& other ) = delete;

  static bool is_idr( const std::string_view nal );

  void unhit_idr() { idr_hit_ = 0; }
};
