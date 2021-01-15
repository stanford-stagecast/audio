#pragma once

#include "eventloop.hh"
#include "formats.hh"
#include "socket.hh"
#include "typed_ring_buffer.hh"

class PartialFrameStore : public EndlessBuffer<std::optional<AudioFrame>>
{
  using parent = EndlessBuffer<std::optional<AudioFrame>>;

public:
  using parent::parent;

  bool has_value( const size_t pos ) const
  {
    if ( pos < range_begin() or pos >= range_end() ) {
      return false;
    }
    return at( pos ).has_value();
  }
};

class NetworkReceiver
{
  PartialFrameStore frames_ { 8192 };
  uint32_t next_frame_needed_ {};
  uint32_t unreceived_beyond_this_frame_index_ {};

  std::optional<uint32_t> biggest_seqno_received_ {};

  TypedRingBuffer<Packet::Record> recent_packets_ { 512 };

  void discard_frames( const unsigned int num );
  void advance_next_frame_needed();

public:
  struct Statistics
  {
    unsigned int already_acked, redundant, dropped, popped;
    std::optional<uint64_t> last_new_frame_received;
  };

private:
  Statistics stats_ {};

public:
  void receive_sender_section( const Packet::SenderSection& sender_section );
  void set_receiver_section( Packet::ReceiverSection& receiver_section );

  void summary( std::ostream& out ) const;

  uint32_t next_frame_needed() const { return next_frame_needed_; }
  uint32_t unreceived_beyond_this_frame_index() const { return unreceived_beyond_this_frame_index_; }

  const PartialFrameStore& frames() const { return frames_; }
  void pop_frames( const size_t num );

  uint32_t biggest_seqno_received() const { return biggest_seqno_received_.value(); }

  const Statistics& stats() const { return stats_; }
};
