#pragma once

#include <chrono>
#include <ostream>

#include "clock.hh"
#include "connection.hh"
#include "cursor.hh"
#include "keys.hh"

static constexpr uint8_t MAX_CLIENTS = 12;

class KnownClient;

class Client
{
  NetworkConnection connection_;
  Clock clock_;
  Cursor cursor_;
  AudioBuffer decoded_audio_ { 8192 };
  AudioBuffer mixed_audio_ { 8192 };

  uint64_t mix_cursor_ {};
  std::optional<uint32_t> outbound_frame_offset_ {};

  uint64_t server_mix_cursor() const;
  uint64_t client_mix_cursor() const;

  OpusEncoderProcess encoder_ { 64000, 600, 48000 };

  using mix_gain = std::pair<float, float>;
  std::array<mix_gain, 2 * MAX_CLIENTS> gains_ {};

public:
  Client( const uint8_t node_id, CryptoSession&& crypto );

  bool receive_packet( const Address& source, const Ciphertext& ciphertext, const uint64_t clock_sample );
  void decode_audio( const uint64_t clock_sample, const uint64_t cursor_sample );
  void mix_and_encode( const std::vector<KnownClient>& clients, const uint64_t cursor_sample );
  void send_packet( UDPSocket& socket );
  void pop_decoded_audio( const uint64_t cursor_sample );

  void summary( std::ostream& out ) const;

  uint8_t node_id() const { return connection().node_id(); }
  uint8_t peer_id() const { return connection().peer_id(); }

  const NetworkConnection& connection() const { return connection_; }
};

class KnownClient
{
  char id_;

  std::string name_;
  CryptoSession long_lived_crypto_;
  std::chrono::steady_clock::time_point next_reply_allowed_;

  std::optional<Client> current_session_ {};

  KeyPair next_keys_ {};
  std::optional<CryptoSession> next_session_;

  struct Statistics
  {
    unsigned int key_requests, key_responses, new_sessions;
  } stats_ {};

public:
  KnownClient( const uint8_t node_id, const LongLivedKey& key );
  bool try_keyrequest( const Address& src, const Ciphertext& ciphertext, UDPSocket& socket );
  void receive_packet( const Address& src, const Ciphertext& ciphertext, const uint64_t clock_sample );

  operator bool() const { return current_session_.has_value(); }
  Client& client() { return current_session_.value(); }
  const Client& client() const { return current_session_.value(); }
  uint8_t id() const { return id_; }

  void clear_current_session() { current_session_.reset(); }

  void summary( std::ostream& out ) const;
};
