#pragma once

#include <chrono>
#include <ostream>
#include <vector>

#include "connection.hh"
#include "cursor.hh"
#include "keys.hh"

class KnownClient;

class AudioBoard
{
  std::vector<std::string> channel_names_;
  std::vector<AudioBuffer> decoded_audio_;

public:
  AudioBoard( const uint8_t num_channels );

  void set_name( const uint8_t ch_num, const std::string_view name ) { channel_names_.at( ch_num ) = name; }

  const AudioChannel& channel( const uint8_t ch_num ) const;
  AudioBuffer& buffer( const uint8_t ch1_num, const uint8_t ch2_num );
  void pop_samples_until( const uint64_t sample );

  uint8_t num_channels() const { return channel_names_.size(); }
  const std::string& channel_name( const uint8_t num ) const { return channel_names_.at( num ); }
};

class Client
{
  NetworkConnection connection_;
  Cursor cursor_;
  OpusDecoderProcess decoder_ {};
  AudioBuffer mixed_audio_ { 8192 };

  uint64_t mix_cursor_ {};
  std::optional<uint32_t> outbound_frame_offset_ {};

  uint64_t server_mix_cursor() const;
  uint64_t client_mix_cursor() const;

  OpusEncoderProcess encoder_ { 96000, 600, 48000 };

  uint8_t ch1_num_, ch2_num_;

public:
  Client( const uint8_t node_id, const uint8_t ch1, const uint8_t ch2, CryptoSession&& crypto );

  using mix_gain = std::pair<float, float>;

  bool receive_packet( const Address& source, const Ciphertext& ciphertext, const uint64_t clock_sample );
  void decode_audio( const uint64_t cursor_sample, AudioBoard& board );
  void mix_and_encode( const std::vector<mix_gain>& gains, const AudioBoard& board, const uint64_t cursor_sample );
  void send_packet( UDPSocket& socket );

  void summary( std::ostream& out ) const;

  uint8_t node_id() const { return connection().node_id(); }
  uint8_t peer_id() const { return connection().peer_id(); }

  const NetworkConnection& connection() const { return connection_; }

  void set_cursor_lag( const uint16_t num_samples );
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

  std::vector<Client::mix_gain> gains_ {};

  uint8_t ch1_num_, ch2_num_;

public:
  KnownClient( const uint8_t node_id,
               const uint8_t num_channels,
               const uint8_t ch1_num,
               const uint8_t ch2_num,
               const LongLivedKey& key );
  bool try_keyrequest( const Address& src, const Ciphertext& ciphertext, UDPSocket& socket );
  void receive_packet( const Address& src, const Ciphertext& ciphertext, const uint64_t clock_sample );

  operator bool() const { return current_session_.has_value(); }
  Client& client() { return current_session_.value(); }
  const Client& client() const { return current_session_.value(); }
  const std::string& name() const { return name_; }
  uint8_t id() const { return id_; }

  void clear_current_session() { current_session_.reset(); }

  void summary( std::ostream& out ) const;

  const std::vector<Client::mix_gain>& gains() const { return gains_; }
};
