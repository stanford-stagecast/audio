#pragma once

#include <bitset>
#include <sstream>

#include "encoder_task.hh"
#include "formats.hh"
#include "socket.hh"
#include "typed_ring_buffer.hh"

class NetworkSender
{
  struct AudioFrameStatus
  {
    bool outstanding : 1;
    bool in_flight : 1;

    bool needs_send() const { return outstanding and not in_flight; }
  };

  static_assert( sizeof( AudioFrameStatus ) == 1 );

  EndlessBuffer<AudioFrame> frames_ { 8192 }; // 20.48 seconds
  EndlessBuffer<AudioFrameStatus> frame_status_ { 8192 };
  uint32_t next_frame_index_ {};
  uint32_t frames_dropped_ {};

  EndlessBuffer<Packet::Record> packets_in_flight_ { 1024 };
  uint32_t next_sequence_number_ {};

  Address server_;
  UDPSocket socket_ {};

  std::shared_ptr<OpusEncoderProcess> source_;

  void push_one_frame();

  void send_packet();

public:
  NetworkSender( const Address& server, std::shared_ptr<OpusEncoderProcess> source, EventLoop& loop );

  void generate_statistics( std::ostringstream& out ) const;
};
