#pragma once

#include <ostream>

#include "encoder_task.hh"
#include "formats.hh"
#include "typed_ring_buffer.hh"

template<class FrameType>
class NetworkSender
{
  struct FrameStatus
  {
    bool outstanding : 1;
    bool in_flight : 1;

    bool needs_send() const { return outstanding and not in_flight; }
  };

  EndlessBuffer<FrameType> frames_ { 8192 }; // 20.48 seconds
  EndlessBuffer<FrameStatus> frame_status_ { 8192 };
  uint32_t next_frame_index_ {};

  constexpr static uint8_t reorder_window = 2; /* 2 packets, about 5 ms */
  std::optional<uint32_t> greatest_sack_ {};
  uint32_t departure_adjudicated_until_seqno() const;

  struct PacketSentRecord
  {
    typename Packet<FrameType>::Record record;
    uint64_t sent_timestamp;
    bool acked : 1;
    bool assumed_lost : 1;
  };

  EndlessBuffer<PacketSentRecord> packets_in_flight_ { 512 };
  uint32_t next_sequence_number_ {};

  bool need_immediate_send_ {};

  void assume_departed( const PacketSentRecord& pack, const bool is_loss );

public:
  struct Statistics
  {
    static constexpr float SRTT_ALPHA = 1 / 100.0;

    unsigned int frames_dropped {}, empty_packets {}, bad_acks {}, packet_transmissions {},
      packet_losses_detected {}, packet_loss_false_positives {}, frames_departed_by_expiration {},
      invalid_timestamp {};

    float smoothed_rtt {};

    unsigned int packet_losses() const { return packet_losses_detected - packet_loss_false_positives; }

    uint64_t last_good_ack_ts = Timer::timestamp_ns();
  };

private:
  Statistics stats_ {};

public:
  template<class SourceType>
  void push_frame( SourceType& encoder )
  {
    if ( frames_.range_begin() != frame_status_.range_begin() ) {
      throw std::runtime_error( "NetworkSender internal error" );
    }

    if ( next_frame_index_ < frames_.range_begin() ) {
      throw std::runtime_error( "NetworkSender internal error: next_frame_index_ < frames_.range_begin()" );
    }

    if ( need_immediate_send_ ) {
      throw std::runtime_error( "packet pushed but not sent" );
    }

    if ( next_frame_index_ >= frames_.range_end() ) {
      const size_t frames_to_drop = next_frame_index_ - frames_.range_end() + 1;
      frames_.pop( frames_to_drop );
      frame_status_.pop( frames_to_drop );
      stats_.frames_dropped += frames_to_drop;
    }

    frames_.at( next_frame_index_ ) = encoder.front( next_frame_index_ );
    frame_status_.at( next_frame_index_ ) = { true, false };
    next_frame_index_++;

    need_immediate_send_ = true;

    encoder.pop_frame();
  }

  void set_sender_section( typename Packet<FrameType>::SenderSection& p );
  void receive_receiver_section( const typename Packet<FrameType>::ReceiverSection& receiver_section );

  void summary( std::ostream& out ) const;

  const Statistics& stats() const { return stats_; }
};
