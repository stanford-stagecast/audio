#pragma once

#include <ostream>

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

  std::vector<KnownClient> clients_ {};

  struct Stats
  {
    unsigned int bad_packets;
  } stats_ {};

  void summary( std::ostream& out ) const override;

public:
  NetworkMultiServer( EventLoop& loop );
  void add_key( const LongLivedKey& key );
};
