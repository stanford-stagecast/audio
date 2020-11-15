#pragma once

#include <sstream>

#include "encoder_task.hh"
#include "formats.hh"
#include "socket.hh"
#include "typed_ring_buffer.hh"

class NetworkSender
{
  EndlessBuffer<std::optional<AudioFrame>> frames_outstanding_ { 8192 }; // 20.48 seconds
  Address server_;
  UDPSocket socket_ {};
  std::shared_ptr<OpusEncoderProcess> encoder_;

  size_t frames_dropped_ {};

  void push_one_frame( OpusEncoderProcess& encoder );

public:
  NetworkSender( const Address& server, std::shared_ptr<OpusEncoderProcess> encoder, EventLoop& loop );

  void generate_statistics( std::ostringstream& out );
};
