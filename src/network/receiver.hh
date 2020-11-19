#pragma once

#include "eventloop.hh"
#include "formats.hh"
#include "socket.hh"
#include "typed_ring_buffer.hh"

class NetworkReceiver
{
  EndlessBuffer<std::optional<AudioFrame>> frames_ { 8192 };
  uint32_t next_frame_needed_ {};
  uint32_t unreceived_beyond_this_frame_index_ {};

  std::optional<uint32_t> biggest_seqno_received_ {};

  TypedRingBuffer<Packet::Record> recent_packets_ { 512 };

  void discard_frames( const unsigned int num );
  void advance_next_frame_needed();

  struct FrameStatistics
  {
    unsigned int already_acked, redundant, dropped, popped;
    std::optional<uint64_t> last_new_frame_received;
  } stats_ {};

public:
  void receive_sender_section( const Packet::SenderSection& sender_section );
  void set_receiver_section( Packet::ReceiverSection& receiver_section );

  void generate_statistics( std::ostream& out ) const;

  uint32_t next_frame_needed() const { return next_frame_needed_; }
  uint32_t unreceived_beyond_this_frame_index() const { return unreceived_beyond_this_frame_index_; }
  const EndlessBuffer<std::optional<AudioFrame>>& frames() const { return frames_; }
  void pop_frames( const size_t num );

  const FrameStatistics& statistics() const { return stats_; }
};
