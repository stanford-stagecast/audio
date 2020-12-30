#pragma once

#include "clock.hh"
#include "connection.hh"
#include "crypto.hh"
#include "cursor.hh"
#include "encoder_task.hh"
#include "wavwriter.hh"

class NetworkClient : public NetworkConnection
{
  UDPSocket socket_;
  std::shared_ptr<OpusEncoderProcess> source_;
  std::shared_ptr<AudioDeviceTask> dest_;

  Clock peer_clock_;
  Cursor cursor_;

  uint64_t next_cursor_sample_;

public:
  NetworkClient( const uint8_t node_id,
                 const Address& server,
                 const Base64Key& send_key,
                 const Base64Key& receive_key,
                 std::shared_ptr<OpusEncoderProcess> source,
                 std::shared_ptr<AudioDeviceTask> dest,
                 EventLoop& loop );
};

class NetworkSingleServer : public NetworkConnection
{
  UDPSocket socket_;

  uint64_t global_ns_timestamp_at_creation_;
  uint64_t next_cursor_sample_;

  Clock peer_clock_;
  Cursor cursor_;

  AudioBuffer mixed_audio_ { 8192 };
  uint64_t mix_cursor_ {};

  std::optional<uint32_t> outbound_frame_offset_ {};
  OpusEncoderProcess encoder_;

  uint64_t server_clock() const;

  NetworkSingleServer( EventLoop& loop, const Base64Key& send_key, const Base64Key& receive_key );

  uint64_t server_mix_cursor() const;
  uint64_t client_mix_cursor() const;

public:
  NetworkSingleServer( EventLoop& loop );

  void summary( std::ostream& out ) const override;
};
