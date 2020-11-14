#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#include "alsa_devices.hh"
#include "audio_device_claim.hh"
#include "eventloop.hh"
#include "exception.hh"

#include "audio_task.hh"
#include "encoder_task.hh"
#include "stats_printer.hh"

using namespace std;
using namespace std::chrono;

void program_body()
{
  ios::sync_with_stdio( false );

  auto loop = make_shared<EventLoop>();

  const auto [name, interface_name] = ALSADevices::find_device( "UAC-2, USB Audio" );
  const auto device_claim = AudioDeviceClaim::try_claim( name );

  auto uac2 = make_shared<AudioDeviceTask>( interface_name, *loop );

  OpusEncoderTask encoder { 128000, 48000, uac2, *loop };

  loop->add_rule(
    "pop Opus frames", [&] { encoder.pop_frame(); }, [&] { return encoder.has_frame(); } );

  StatsPrinterTask stats_printer { uac2, loop };

  uac2->device().start();
  while ( loop->wait_next_event( -1 ) != EventLoop::Result::Exit ) {
  }
}

int main()
{
  try {
    program_body();
    cout << global_timer().summary() << "\n";
  } catch ( const exception& e ) {
    cout << "Exception: " << e.what() << "\n";
    cout << global_timer().summary() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
