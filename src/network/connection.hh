#pragma once

#include <array>
#include <memory>
#include <ostream>

#include "address.hh"
#include "crypto.hh"
#include "receiver.hh"
#include "sender.hh"
#include "socket.hh"
#include "summarize.hh"

class OpusEncoderProcess;

class NetworkConnection : public Summarizable
{
  char node_id_, peer_id_;

  NetworkSender sender_ {};
  NetworkReceiver receiver_ {};

  CryptoSession crypto_;

  bool auto_home_;
  std::optional<Address> destination_;
  std::optional<uint32_t> last_biggest_seqno_received_ {};

  struct Statistics
  {
    unsigned int decryption_failures {}, invalid {};
  } stats_ {};

public:
  NetworkConnection( const char node_id,
                     const char peer_id,
                     const Base64Key& encrypt_key,
                     const Base64Key& decrypt_key );

  NetworkConnection( const char node_id,
                     const char peer_id,
                     const Base64Key& encrypt_key,
                     const Base64Key& decrypt_key,
                     const Address& destination );

  void set_destination( const Address& destination ) { destination_ = destination; }
  bool has_destination() const { return destination_.has_value(); }

  void push_frame( OpusEncoderProcess& source ) { sender_.push_frame( source ); }
  void summary( std::ostream& out ) const override;

  uint32_t next_frame_needed() const { return receiver_.next_frame_needed(); }
  uint32_t unreceived_beyond_this_frame_index() const { return receiver_.unreceived_beyond_this_frame_index(); }
  const EndlessBuffer<std::optional<AudioFrame>>& frames() const { return receiver_.frames(); }
  void pop_frames( const size_t num ) { receiver_.pop_frames( num ); }

  void send_packet( UDPSocket& socket );
  void receive_packet( const Address& source, const Ciphertext& plaintext );
};
