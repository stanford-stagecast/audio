#pragma once

#include "audio_task.hh"
#include "eventloop.hh"
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
