#pragma once

#include <ostream>

#include "clock.hh"
#include "connection.hh"
#include "cursor.hh"

static constexpr uint8_t MAX_CLIENTS = 8;

class Client
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

  OpusEncoderProcess encoder { 128000, 48000 };

  using mix_gain = std::pair<float, float>;
  std::array<mix_gain, 2 * MAX_CLIENTS> gains {};

  Client( const uint8_t node_id,
          const uint16_t server_port,
          const Base64Key& send_key,
          const Base64Key& receive_key );

public:
  Client( const uint8_t node_id, const uint16_t server_port );

  void receive_packet( const Address& source, const Ciphertext& ciphertext, const uint64_t clock_sample );
  void decode_audio( const uint64_t clock_sample, const uint64_t cursor_sample );
  void mix_and_encode( const std::vector<Client>& clients, const uint64_t cursor_sample );
  void send_packet( UDPSocket& socket );
  void pop_decoded_audio( const uint64_t cursor_sample );

  void summary( std::ostream& out ) const;
};
