#pragma once

#include <chrono>
#include <ostream>
#include <string>

#include "alsa_devices.hh"
#include "eventloop.hh"
#include "summarize.hh"

class AudioDeviceTask : public Summarizable
{
  AudioPair device_;
  AudioBuffer capture_ { 65536 }, playback_ { 65536 };
  void service_device();
  void install_rules( EventLoop& loop );

public:
  AudioDeviceTask( const std::string_view interface_name, EventLoop& loop );
  void summary( std::ostream& out ) const override;
  void reset_summary() override { device_.reset_statistics(); }

  AudioPair& device() { return device_; }
  AudioBuffer& capture() { return capture_; }
  AudioBuffer& playback() { return playback_; }

  const AudioPair& device() const { return device_; }
  const AudioBuffer& capture() const { return capture_; }
  const AudioBuffer& playback() const { return playback_; }

  size_t cursor() const { return device().cursor(); }
};
