#pragma once

#include <chrono>
#include <sstream>
#include <string>

#include "alsa_devices.hh"
#include "eventloop.hh"

class AudioDeviceTask
{
  AudioPair device_;
  AudioBuffer capture_ { 65536 }, playback_ { 65536 };
  void service_device();
  void install_rules( EventLoop& loop );

public:
  AudioDeviceTask( const std::string_view interface_name, EventLoop& loop );
  void generate_statistics( std::ostringstream& out );

  AudioPair& device() { return device_; }
  AudioBuffer& capture() { return capture_; }
  AudioBuffer& playback() { return playback_; }
};
