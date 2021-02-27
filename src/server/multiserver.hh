#pragma once

#include <ostream>

#include <json/json.h>

#include "client.hh"
#include "summarize.hh"

class NetworkMultiServer : public Summarizable
{
  static constexpr uint64_t CLIENT_TIMEOUT_NS = 4'000'000'000;

  UDPSocket socket_;
  uint64_t global_ns_timestamp_at_creation_;
  uint64_t next_cursor_sample_;
  uint64_t server_clock() const;

  void receive_keyrequest( const Address& src, const Ciphertext& ciphertext );

  uint8_t num_clients_;

  AudioBoard internal_board_, program_board_;
  std::vector<KnownClient> clients_ {};

  struct Stats
  {
    unsigned int bad_packets;
  } stats_ {};

  AudioWriter program_audio_ { "stagecast-program-audio" };

public:
  NetworkMultiServer( const uint8_t num_clients, EventLoop& loop );
  void add_key( const LongLivedKey& key );

  void set_cursor_lag( const std::string_view name,
                       const std::string_view feed,
                       const uint16_t target_samples,
                       const uint16_t min_samples,
                       const uint16_t max_samples );
  void set_gain( const std::string_view board_name,
                 const std::string_view channel_name,
                 const float gain1,
                 const float gain2 );

  void initialize_clock();

  void summary( std::ostream& out ) const override;
  void json_summary( Json::Value& root ) const;
};
