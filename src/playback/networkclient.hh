#pragma once

#include <chrono>

#include "connection.hh"
#include "cursor.hh"
#include "encoder_task.hh"
#include "keys.hh"

#include <rubberband/RubberBandStretcher.h>

class NetworkClient : public Summarizable
{
  struct NetworkSession
  {
    AudioNetworkConnection connection;
    Cursor cursor;

    NetworkSession( const uint8_t node_id, const KeyPair& session_key, const Address& destination );

    void transmit_frame( OpusEncoderProcess& source, UDPSocket& socket );
    void network_receive( const Ciphertext& ciphertext );
    void decode( const size_t decode_cursor,
                 OpusDecoderProcess& decoder,
                 RubberBand::RubberBandStretcher& stretcher,
                 ChannelPair& output );
    void summary( std::ostream& out ) const;
    void json_summary( Json::Value& root ) const { cursor.json_summary( root ); }
  };

  UDPSocket socket_ {};
  Address server_;

  std::string name_;
  CryptoSession long_lived_crypto_;

  std::optional<NetworkSession> session_ {};
  OpusDecoderProcess decoder_ { false };
  RubberBand::RubberBandStretcher stretcher_;

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
  void json_summary( Json::Value& root ) const;

  void set_cursor_lag( const uint16_t target_samples, const uint16_t min_samples, const uint16_t max_samples );
};
