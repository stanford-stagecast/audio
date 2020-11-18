#include <cstdlib>
#include <iostream>

#include "alsa_devices.hh"
#include "audio_device_claim.hh"
#include "audio_task.hh"
#include "connection.hh"
#include "encoder_task.hh"
#include "eventloop.hh"
#include "stats_printer.hh"

using namespace std;

void program_body()
{
  ios::sync_with_stdio( false );

  auto loop = make_shared<EventLoop>();

  /* Network server registeres itself in EventLoop */
  auto network_server = make_shared<NetworkServer>( *loop );

  /* Print out statistics to terminal */
  StatsPrinterTask stats_printer { shared_ptr<AudioDeviceTask> {}, network_server, loop };

  /* Start audio device and event loop */
  while ( loop->wait_next_event( -1 ) != EventLoop::Result::Exit ) {
  }
}

int main()
{
  try {
    program_body();
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << "\n";
    global_timer().summary( cerr );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
