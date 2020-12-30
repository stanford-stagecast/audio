#pragma once

#include <chrono>
#include <memory>
#include <sstream>
#include <unistd.h>

#include "audio_task.hh"
#include "connection.hh"
#include "eventloop.hh"
#include "file_descriptor.hh"
#include "ring_buffer.hh"

class StatsPrinterTask
{
  std::shared_ptr<EventLoop> loop_;
  std::vector<std::shared_ptr<Summarizable>> objects_ {};

  FileDescriptor standard_output_;
  RingBuffer output_rb_ { 65536 };

  using time_point = decltype( std::chrono::steady_clock::now() );

  time_point next_stats_print, next_stats_reset;

  static constexpr auto stats_print_interval = std::chrono::milliseconds( 500 );
  static constexpr auto stats_reset_interval = std::chrono::seconds( 10 );

  std::ostringstream ss_ {};

public:
  StatsPrinterTask( std::shared_ptr<EventLoop> loop,
                    const std::chrono::nanoseconds initial_delay = std::chrono::seconds( 0 ) );

  unsigned int wait_time_ms() const;

  template<class T>
  void add( std::shared_ptr<T> obj )
  {
    objects_.push_back( std::static_pointer_cast<Summarizable>( obj ) );
  }
};
