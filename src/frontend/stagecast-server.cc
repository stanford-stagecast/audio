#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

#include "alsa_devices.hh"
#include "audio_device_claim.hh"
#include "audio_task.hh"
#include "encoder_task.hh"
#include "eventloop.hh"
#include "multiserver.hh"
#include "stats_printer.hh"

using namespace std;
using namespace std::chrono;

void program_body( const vector<string>& keyfiles )
{
  ios::sync_with_stdio( false );

  auto loop = make_shared<EventLoop>();

  /* Network server registeres itself in EventLoop */
  auto server = make_shared<NetworkMultiServer>( keyfiles.size(), *loop );

  for ( const auto& filename : keyfiles ) {
    ReadOnlyFile file { filename };
    Parser p { file };
    server->add_key( LongLivedKey { p } );
  }

  /* Print out statistics to terminal */
  StatsPrinterTask stats_printer { loop };
  stats_printer.add( server );

  /* Start audio device and event loop */
  while ( loop->wait_next_event( 1 ) != EventLoop::Result::Exit ) {
  }
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }

    if ( argc <= 1 ) {
      cerr << "Usage: " << argv[0] << " keyfile...\n";
      return EXIT_FAILURE;
    }

    vector<string> keys;
    for ( int i = 1; i < argc; i++ ) {
      keys.push_back( argv[i] );
    }
    program_body( keys );
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << "\n";
    global_timer().summary( cerr );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
