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

class NetworkEndpoint
{
  NetworkSender sender_ {};
  NetworkReceiver receiver_ {};

  struct Statistics
  {
    unsigned int invalid {};
  } stats_ {};

protected:
  UDPSocket socket_;

  void send_packet( const Address& dest );
  Address receive_packet();

  Base64Key send_key_, receive_key_;
  Session crypto_ { send_key_, receive_key_ };

public:
  NetworkEndpoint();
  NetworkEndpoint( const Base64Key& send_key, const Base64Key& receive_key );

  void push_frame( OpusEncoderProcess& source );
  void generate_statistics( std::ostream& out ) const;

  uint32_t range_begin() const { return receiver_.range_begin(); }
  span_view<std::optional<AudioFrame>> received_frames() const { return receiver_.received_frames(); }
  void pop_frames( const size_t num ) { return receiver_.pop_frames( num ); }
};

class NetworkClient : public NetworkEndpoint
{
  Address server_;
  std::shared_ptr<OpusEncoderProcess> source_;

public:
  NetworkClient( const Address& server,
                 const Base64Key& send_key,
                 const Base64Key& receive_key,
                 std::shared_ptr<OpusEncoderProcess> source,
                 EventLoop& loop );
};

class NetworkServer : public NetworkEndpoint
{
  std::optional<Address> peer_;

public:
  NetworkServer( EventLoop& loop );
};
