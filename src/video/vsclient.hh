#pragma once

#include <chrono>
#include <ostream>
#include <vector>

#include "audioboard.hh"
#include "connection.hh"
#include "crypto.hh"
#include "cursor.hh"
#include "keys.hh"
#include "videoclient.hh"

class VSClient
{
  VideoNetworkConnection connection_;

public:
  VSClient( const uint8_t node_id, CryptoSession&& crypto );

  FileDescriptor output_;

  bool receive_packet( const Address& source, const Ciphertext& ciphertext );
  void send_packet( UDPSocket& sock );

  void summary( std::ostream& out ) const;

  uint8_t node_id() const { return connection().node_id(); }
  uint8_t peer_id() const { return connection().peer_id(); }

  const VideoNetworkConnection& connection() const { return connection_; }
};

class KnownVideoClient
{
  char id_;

  std::string name_;
  CryptoSession long_lived_crypto_;
  std::chrono::steady_clock::time_point next_reply_allowed_;

  std::optional<VSClient> current_session_ {};

  KeyPair next_keys_ {};
  std::optional<CryptoSession> next_session_;

  struct Statistics
  {
    unsigned int key_requests, key_responses, new_sessions;
  } stats_ {};

public:
  KnownVideoClient( const uint8_t node_id, const LongLivedKey& key );
  bool try_keyrequest( const Address& src, const Ciphertext& ciphertext, UDPSocket& socket );
  void receive_packet( const Address& src, const Ciphertext& ciphertext, const uint64_t clock_sample );

  operator bool() const { return current_session_.has_value(); }
  VSClient& client() { return current_session_.value(); }
  const VSClient& client() const { return current_session_.value(); }
  const std::string& name() const { return name_; }
  uint8_t id() const { return id_; }

  void clear_current_session() { current_session_.reset(); }

  void summary( std::ostream& out ) const;
};
