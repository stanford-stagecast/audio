#pragma once

#include "connection.hh"

#include <vector>

class NetworkMultiServer
{
  struct Client
  {
    Address addr;
    NetworkEndpoint endpoint;
  };

  UDPSocket socket_;
  std::vector<Client> clients_ {};

  Base64Key send_key_ {}, receive_key_ {};
  Session crypto_ { send_key_, receive_key_ };

public:
  NetworkMultiServer( EventLoop& loop );
};
