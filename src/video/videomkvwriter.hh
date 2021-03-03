#pragma once

#include <memory>
#include <string>

extern "C"
{
#include <libavformat/avformat.h>
}

#include "exception.hh"
#include "file_descriptor.hh"
#include "ring_buffer.hh"

class VideoMKVWriter
{
  struct avformat_deleter
  {
    void operator()( AVFormatContext* x ) const { avformat_free_context( x ); }
  };
  std::unique_ptr<AVFormatContext, avformat_deleter> context_ {};

  uint8_t* buffer_ {};

  AVStream *audio_stream_, *video_stream_;
  std::string video_extradata_ {};
  bool idr_hit_ {};

  static constexpr unsigned int BUF_SIZE = 1048576;
  RingBuffer buf_ { BUF_SIZE };

  constexpr static unsigned int MKV_TIMEBASE = 90000;

  unsigned int sample_rate_;
  unsigned int frame_rate_, width_, height_;

  uint64_t sample_count_ {};

  static bool is_idr( const std::string_view nal );

public:
  VideoMKVWriter( const int audio_bit_rate,
                  const uint32_t audio_sample_rate,
                  const uint8_t audio_num_channels,
                  const unsigned int video_frame_rate,
                  const unsigned int width,
                  const unsigned int height );

  uint64_t write_audio( const std::string_view frame, const uint16_t num_samples );

  uint64_t write_video( const std::string_view nal, const uint32_t presentation_no, const uint32_t display_no );

  RingBuffer& output() { return buf_; }

  VideoMKVWriter( const VideoMKVWriter& other ) = delete;
  VideoMKVWriter& operator=( const VideoMKVWriter& other ) = delete;
};
