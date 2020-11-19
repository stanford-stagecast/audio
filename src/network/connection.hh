#pragma once

#include <array>
#include <memory>
#include <ostream>

#include "address.hh"
#include "base64.hh"
#include "crypto.hh"
#include "encoder_task.hh"
#include "receiver.hh"
#include "sender.hh"
#include "socket.hh"
#include "summarize.hh"

class NetworkEndpoint : public Summarizable
{
  NetworkSender sender_ {};
  NetworkReceiver receiver_ {};

  struct Statistics
  {
    unsigned int decryption_failures {}, invalid {};
  } stats_ {};

public:
  void push_frame( OpusEncoderProcess& source );
  void summary( std::ostream& out ) const override;

  uint32_t next_frame_needed() const { return receiver_.next_frame_needed(); }
  uint32_t unreceived_beyond_this_frame_index() const { return receiver_.unreceived_beyond_this_frame_index(); }
  const EndlessBuffer<std::optional<AudioFrame>>& frames() const { return receiver_.frames(); }
  void pop_frames( const size_t num ) { receiver_.pop_frames( num ); }

  void send_packet( Session& crypto_session, const Address& dest, UDPSocket& socket );
  void receive_packet( Plaintext& plaintext );

  void decryption_failure() { stats_.decryption_failures++; }
};

class NetworkClient : public NetworkEndpoint
{
  UDPSocket socket_;
  Address server_;
  std::shared_ptr<OpusEncoderProcess> source_;

  Session crypto_;

public:
  NetworkClient( const Address& server,
                 const Base64Key& send_key,
                 const Base64Key& receive_key,
                 std::shared_ptr<OpusEncoderProcess> source,
                 EventLoop& loop );
};

class NetworkSingleServer : public NetworkEndpoint
{
  UDPSocket socket_;
  Address peer_;

  Base64Key send_key_ {}, receive_key_ {};
  Session crypto_ { send_key_, receive_key_ };

public:
  NetworkSingleServer( EventLoop& loop );
};
