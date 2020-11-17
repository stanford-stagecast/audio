#pragma once

#include <ostream>

#include "encoder_task.hh"
#include "formats.hh"
#include "typed_ring_buffer.hh"

class NetworkSender
{
  struct AudioFrameStatus
  {
    bool outstanding : 1;
    bool in_flight : 1;

    bool needs_send() const { return outstanding and not in_flight; }
  };

  EndlessBuffer<AudioFrame> frames_ { 8192 }; // 20.48 seconds
  EndlessBuffer<AudioFrameStatus> frame_status_ { 8192 };
  uint32_t next_frame_index_ {};

  constexpr static uint8_t reorder_window = 8; /* 8 packets, about 20 ms */
  std::optional<uint32_t> greatest_sack_ {};
  uint32_t departure_adjudicated_until_seqno() const;

  EndlessBuffer<Packet::Record> packets_in_flight_ { 512 };
  uint32_t next_sequence_number_ {};

  bool need_immediate_send_ {};

  void assume_departed( const Packet::Record& pack, const bool is_loss );

  struct Statistics
  {
    unsigned int frames_dropped {}, empty_packets {}, bad_acks {}, packet_transmissions {},
      packet_losses_detected {}, packet_loss_false_positives {};

    unsigned int packet_losses() const { return packet_losses_detected - packet_loss_false_positives; }
  } stats_ {};

public:
  void push_frame( OpusEncoderProcess& encoder );

  void set_sender_section( Packet::SenderSection& p );
  void receive_receiver_section( const Packet::ReceiverSection& receiver_section );

  void generate_statistics( std::ostream& out ) const;
};
