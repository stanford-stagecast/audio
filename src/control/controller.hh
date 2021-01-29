#pragma once

#include "audio_task.hh"
#include "eventloop.hh"
#include "multiserver.hh"
#include "networkclient.hh"
#include "socket.hh"

class ClientController
{
  UDPSocket socket_;

  std::shared_ptr<NetworkClient> client_;
  std::shared_ptr<AudioDeviceTask> audio_device_;

public:
  ClientController( std::shared_ptr<NetworkClient> client,
                    std::shared_ptr<AudioDeviceTask> audio_device,
                    EventLoop& loop );
};

class ServerController
{
  UDPSocket socket_;

  std::shared_ptr<NetworkMultiServer> server_;

public:
  ServerController( std::shared_ptr<NetworkMultiServer> client, EventLoop& loop );
};
