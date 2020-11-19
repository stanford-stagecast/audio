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
    Address addr;
    std::unique_ptr<NetworkEndpoint> endpoint { std::make_unique<NetworkEndpoint>() };
    Cursor cursor;

    Client( const Address& s_addr );
    void receive_packet( Plaintext& plaintext );
    void summary( std::ostream& out ) const;
  };

  UDPSocket socket_;
  std::vector<Client> clients_ {};

  Base64Key send_key_ {}, receive_key_ {};
  Session crypto_ { send_key_, receive_key_ };

  void receive_packet();
  void service_client( Client& client, Plaintext& plaintext );

  using time_point = decltype( std::chrono::steady_clock::now() );

  static constexpr uint64_t cursor_sample_interval = 1000000;
  uint64_t next_cursor_sample;

  struct Statistics
  {
    unsigned int decryption_failures;
  } stats_ {};

  void summary( std::ostream& out ) const override;

public:
  NetworkMultiServer( EventLoop& loop );
};
