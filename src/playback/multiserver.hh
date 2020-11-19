#pragma once

#include <ostream>
#include <vector>

#include "connection.hh"
#include "cursor.hh"
#include "summarize.hh"

class NetworkMultiServer : public Summarizable
{
  struct Client
  {
    uint8_t node_id;
    Address addr;
    NetworkEndpoint endpoint { 255 };
    Cursor cursor;

    using mix_gain = std::pair<float, float>;
    std::array<mix_gain, 10> gains {};

    AudioChannel mixed_ch1 { 1024 }, mixed_ch2 { 1024 };
    OpusEncoderProcess encoder_ { 128000, 48000 };

    Client( const uint8_t s_node_id, const Address& s_addr );
    void summary( std::ostream& out ) const;
  };

  UDPSocket socket_;
  std::array<std::optional<Client>, 256> clients_ {};

  Base64Key send_key_ {}, receive_key_ {};
  Session crypto_ { send_key_, receive_key_ };

  void receive_packet();
  void service_client( Client& client, Plaintext& plaintext );

  using time_point = decltype( std::chrono::steady_clock::now() );

  static constexpr uint64_t cursor_sample_interval = 1000000;
  uint64_t next_cursor_sample_;

  size_t global_sample_index_ {};
  size_t next_encode_index_ { 240 };

  struct Statistics
  {
    unsigned int decryption_failures, invalid;
  } stats_ {};

  void summary( std::ostream& out ) const override;

  void mix_and_encode( Client& client );

public:
  NetworkMultiServer( EventLoop& loop );
};
