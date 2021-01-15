#pragma once

#include <ostream>
#include <vector>

#include "clock.hh"
#include "connection.hh"
#include "cursor.hh"
#include "summarize.hh"

class NetworkMultiServer : public Summarizable
{
  UDPSocket socket_;
  uint64_t global_ns_timestamp_at_creation_;
  uint64_t next_cursor_sample_;

  uint64_t server_clock() const;

  static constexpr uint8_t MAX_CLIENTS = 12;

  struct Client
  {
    NetworkConnection connection;
    Clock clock;
    Cursor cursor;
    AudioBuffer decoded_audio { 8192 };
    AudioBuffer mixed_audio { 8192 };

    uint64_t mix_cursor_ {};
    std::optional<uint32_t> outbound_frame_offset_ {};

    uint64_t server_mix_cursor() const;
    uint64_t client_mix_cursor() const;

    OpusEncoderProcess encoder { 96000, 48000 };

    using mix_gain = std::pair<float, float>;
    std::array<mix_gain, 2 * MAX_CLIENTS> gains {};

    Client( const uint8_t node_id, const uint16_t server_port );
    void summary( std::ostream& out ) const;

    Client( const Client& other ) = delete;
    Client& operator=( const Client& other ) const = delete;

  private:
    Client( const uint8_t node_id,
            const uint16_t server_port,
            const Base64Key& send_key,
            const Base64Key& receive_key );
  };

  std::array<std::optional<Client>, MAX_CLIENTS> clients_ {};

  void summary( std::ostream& out ) const override;

public:
  NetworkMultiServer( EventLoop& loop );
};
