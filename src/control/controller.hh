#pragma once

#include "endpoints.hh"
#include "eventloop.hh"
#include "socket.hh"

struct set_cursor_lag
{
  uint16_t num_samples;

  constexpr uint32_t serialized_length() const { return sizeof( num_samples ); }
  void serialize( Serializer& s ) const { s.integer( num_samples ); }
  void parse( Parser& p ) { p.integer( num_samples ); }
};

class ClientController
{
  UDPSocket socket_;

  std::shared_ptr<NetworkClient> client_;

public:
  static constexpr uint16_t control_port() { return 3007; }

  ClientController( std::shared_ptr<NetworkClient> client, EventLoop& loop );
};
