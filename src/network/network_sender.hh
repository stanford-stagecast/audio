#pragma once

#include "address.hh"
#include "socket.hh"

class NetworkSender
{
  UDPSocket socket_ {};
  Address destination_;

public:
  NetworkSender( const Address& destination );
};
