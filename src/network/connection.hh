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

  AudioNetworkSender sender_ {};
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
  NetworkConnection( const char node_id, const char peer_id, CryptoSession&& crypto );
  NetworkConnection( const char node_id, const char peer_id, CryptoSession&& crypto, const Address& destination );

  bool has_destination() const { return destination_.has_value(); }
  const Address& destination() const { return destination_.value(); }

  void push_frame( OpusEncoderProcess& source ) { sender_.push_frame( source ); }
  void summary( std::ostream& out ) const override;

  void send_packet( UDPSocket& socket );
  bool receive_packet( const Ciphertext& ciphertext, const Address& source );
  bool receive_packet( const Ciphertext& ciphertext );

  uint32_t next_frame_needed() const { return receiver_.next_frame_needed(); }
  uint32_t unreceived_beyond_this_frame_index() const { return receiver_.unreceived_beyond_this_frame_index(); }
  const PartialFrameStore& frames() const { return receiver_.frames(); }
  void pop_frames( const size_t num ) { receiver_.pop_frames( num ); }

  uint8_t node_id() const { return node_id_; }
  uint8_t peer_id() const { return peer_id_; }

  const AudioNetworkSender::Statistics& sender_stats() const { return sender_.stats(); }
  const NetworkReceiver::Statistics& receiver_stats() const { return receiver_.stats(); }
};
