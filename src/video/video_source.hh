#pragma once

#include "formats.hh"
#include "h264_encoder.hh"
#include "summarize.hh"
#include "timestamp.hh"
#include "typed_ring_buffer.hh"

#include <queue>
#include <string>
#include <string_view>

class VideoSource : public Summarizable
{
  struct TimedNAL
  {
    uint32_t nal_index;
    uint64_t timestamp_completion;
    size_t offset;
    std::string nal;

    unsigned int num_chunks() const;
    size_t next_chunk_size() const;
    std::string_view next_chunk() const;
    bool last_chunk() const;
  };

  uint64_t beginning_time_ {};
  uint32_t next_nal_index_ {};
  std::queue<TimedNAL> outbound_queue_ {};
  std::optional<uint64_t> timestamp_next_chunk_ {};

public:
  void push( const H264Encoder::EncodedNAL& nal, const uint64_t now );

  uint64_t wait_time_ms( const uint64_t now ) const;
  bool ready( const uint64_t now ) const;

  /* for these methods (used by the templated NetworkSender), "frame" refers to a VideoChunk */
  bool has_frame() const;
  void pop_frame();
  VideoChunk front( const uint32_t frame_index ) const;

  void summary( std::ostream& out ) const override;
};
