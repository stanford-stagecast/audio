#pragma once

#include <chrono>

#include "clock.hh"
#include "connection.hh"
#include "cursor.hh"
#include "encoder_task.hh"
#include "keys.hh"

class NetworkClient : public Summarizable
{
  struct NetworkSession
  {
    NetworkConnection connection;
    Clock peer_clock;
    Cursor cursor;

    NetworkSession( const uint8_t node_id,
                    const KeyPair& session_key,
                    const Address& destination,
                    const size_t audio_cursor );

    void transmit_frame( OpusEncoderProcess& source, UDPSocket& socket );
    void network_receive( const Ciphertext& ciphertext, const size_t audio_cursor );
    void decode( const size_t audio_cursor, const size_t decode_cursor, AudioBuffer& output );
    void summary( std::ostream& out ) const;
  };

  UDPSocket socket_ {};
  Address server_;

  std::string name_;
  CryptoSession long_lived_crypto_;

  std::optional<NetworkSession> session_ {};

  std::shared_ptr<OpusEncoderProcess> source_;

  std::shared_ptr<AudioDeviceTask> dest_;
  size_t decode_cursor_ {};

  void process_keyreply( const Ciphertext& ciphertext );
  std::chrono::steady_clock::time_point next_key_request_;

  struct Statistics
  {
    unsigned int key_requests, new_sessions, bad_packets, timeouts;
  } stats_ {};

public:
  NetworkClient( const Address& server,
                 const LongLivedKey& key,
                 std::shared_ptr<OpusEncoderProcess> source,
                 std::shared_ptr<AudioDeviceTask> dest,
                 EventLoop& loop );

  void summary( std::ostream& out ) const override;
};
