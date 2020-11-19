#pragma once

#include <ostream>
#include <vector>

#include "connection.hh"
#include "summarize.hh"

class NetworkMultiServer : public Summarizable
{
  struct Client
  {
    Address addr;
    std::unique_ptr<NetworkEndpoint> endpoint { std::make_unique<NetworkEndpoint>() };

    Client( const Address& s_addr )
      : addr( s_addr )
    {}
  };

  UDPSocket socket_;
  std::vector<Client> clients_ {};

  Base64Key send_key_ {}, receive_key_ {};
  Session crypto_ { send_key_, receive_key_ };

  void receive_packet();
  void service_client( Client& client, Plaintext& plaintext );

  struct Statistics
  {
    unsigned int decryption_failures;
  } stats_ {};

  void summary( std::ostream& out ) const override;

public:
  NetworkMultiServer( EventLoop& loop );
};
