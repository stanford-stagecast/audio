#pragma once

#include "clock.hh"
#include "connection.hh"
#include "crypto.hh"
#include "cursor.hh"
#include "encoder_task.hh"

class NetworkClient : public NetworkConnection
{
  UDPSocket socket_;
  std::shared_ptr<OpusEncoderProcess> source_;
  std::shared_ptr<AudioDeviceTask> dest_;

  static constexpr uint64_t cursor_sample_interval = 1000000;
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
  uint64_t last_server_clock_sample_;

  Clock peer_clock_;
  Cursor cursor_;

  uint64_t server_clock() const;

  NetworkSingleServer( EventLoop& loop, const Base64Key& send_key, const Base64Key& receive_key );

public:
  NetworkSingleServer( EventLoop& loop );

  void summary( std::ostream& out ) const override;
};
