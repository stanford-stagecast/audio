#pragma once

#include <chrono>
#include <ostream>
#include <vector>

#include "audioboard.hh"
#include "connection.hh"
#include "cursor.hh"
#include "keys.hh"

#include <rubberband/RubberBandStretcher.h>

class AudioFeed
{
  std::string name_;
  Cursor cursor_;
  OpusDecoderProcess decoder_ { true };
  RubberBand::RubberBandStretcher stretcher_;

public:
  AudioFeed( const std::string_view name,
             const uint32_t target_lag_samples,
             const uint32_t min_lag_samples,
             const uint32_t max_lag_samples,
             const bool short_window );

  void summary( std::ostream& out ) const { cursor_.summary( out ); }

  void decode_into( const PartialFrameStore<AudioFrame>& frames,
                    uint64_t cursor_sample,
                    const uint64_t frontier_sample_index,
                    AudioChannel& ch1,
                    AudioChannel& ch2 );

  size_t ok_to_pop( const PartialFrameStore<AudioFrame>& frames ) const { return cursor_.ok_to_pop( frames ); }

  const std::string& name() const { return name_; }

  Cursor& cursor() { return cursor_; }
  const Cursor& cursor() const { return cursor_; }
};

class Client
{
  AudioNetworkConnection connection_;
  AudioFeed internal_feed_, quality_feed_;

  ChannelPair mixed_audio_ { 8192 };

  uint64_t mix_cursor_ {};
  std::optional<uint32_t> outbound_frame_offset_ {};

  uint64_t server_mix_cursor() const;
  uint64_t client_mix_cursor() const;

  OpusEncoderProcess encoder_ { 96000, 48000 };

  uint8_t ch1_num_, ch2_num_;

public:
  Client( const uint8_t node_id, const uint8_t ch1, const uint8_t ch2, CryptoSession&& crypto );

  bool receive_packet( const Address& source, const Ciphertext& ciphertext, const uint64_t clock_sample );
  void decode_audio( const uint64_t cursor_sample, AudioBoard& internal_board, AudioBoard& quality_board );
  void mix_and_encode( const AudioBoard& board, const uint64_t cursor_sample );
  void send_packet( UDPSocket& socket );

  void summary( std::ostream& out ) const;
  void json_summary( Json::Value& root ) const;
  static void default_json_summary( Json::Value& root );

  uint8_t node_id() const { return connection().node_id(); }
  uint8_t peer_id() const { return connection().peer_id(); }

  const AudioNetworkConnection& connection() const { return connection_; }

  void set_cursor_lag( const std::string_view feed,
                       const uint16_t target_samples,
                       const uint16_t min_samples,
                       const uint16_t max_samples );
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

  uint8_t ch1_num_, ch2_num_;

public:
  KnownClient( const uint8_t node_id, const uint8_t ch1_num, const uint8_t ch2_num, const LongLivedKey& key );
  bool try_keyrequest( const Address& src, const Ciphertext& ciphertext, UDPSocket& socket );
  void receive_packet( const Address& src, const Ciphertext& ciphertext, const uint64_t clock_sample );

  operator bool() const { return current_session_.has_value(); }
  Client& client() { return current_session_.value(); }
  const Client& client() const { return current_session_.value(); }
  const std::string& name() const { return name_; }
  uint8_t id() const { return id_; }

  void clear_current_session() { current_session_.reset(); }

  void summary( std::ostream& out ) const;
};
