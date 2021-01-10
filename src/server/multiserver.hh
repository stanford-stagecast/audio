#pragma once

#include <ostream>

#include "client.hh"
#include "summarize.hh"

class NetworkMultiServer : public Summarizable
{
  UDPSocket socket_;
  uint64_t global_ns_timestamp_at_creation_;
  uint64_t next_cursor_sample_;

  uint64_t server_clock() const;

  std::array<std::optional<Client>, MAX_CLIENTS> clients_ {};

  void summary( std::ostream& out ) const override;

public:
  NetworkMultiServer( EventLoop& loop );
};
