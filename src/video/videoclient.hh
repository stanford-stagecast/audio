#pragma once

#include <chrono>

#include "connection.hh"
#include "control_messages.hh"
#include "keys.hh"
#include "video_source.hh"

using VideoNetworkConnection = NetworkConnection<VideoChunk, VideoSource>;

class VideoClient : public Summarizable
{
  struct NetworkSession
  {
    VideoNetworkConnection connection;

    NetworkSession( const uint8_t node_id, const KeyPair& session_key, const Address& destination );

    void transmit_frame( VideoSource& source, UDPSocket& socket );
    void network_receive( const Ciphertext& ciphertext );
    void decode();
    void summary( std::ostream& out ) const;

    std::optional<video_control> control {};
  };

  UDPSocket socket_ {};
  Address server_;

  std::string name_;
  CryptoSession long_lived_crypto_;

  std::optional<NetworkSession> session_ {};
  /* H264Decoder */

  std::shared_ptr<VideoSource> source_;

  void process_keyreply( const Ciphertext& ciphertext );
  std::chrono::steady_clock::time_point next_key_request_;

  struct Statistics
  {
    unsigned int key_requests, new_sessions, bad_packets, timeouts;
  } stats_ {};

public:
  VideoClient( const Address& server,
               const LongLivedKey& key,
               std::shared_ptr<VideoSource> source,
               EventLoop& loop );

  void summary( std::ostream& out ) const override;

  uint64_t wait_time_ms( const uint64_t now ) const { return source_->wait_time_ms( now ); }

  bool has_control() const { return session_.has_value() and session_.value().control.has_value(); }
  const video_control& control() { return session_.value().control.value(); }
  void pop_control() { session_.value().control.reset(); }
};
