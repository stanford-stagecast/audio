#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

#include "alsa_devices.hh"
#include "audio_device_claim.hh"
#include "audio_task.hh"
#include "controller.hh"
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
    LongLivedKey key { p };
    bool takes_program_audio = false;
    if ( filename.size() > 3 and filename.substr( filename.size() - 3 ) == "prg" ) {
      takes_program_audio = true;
    }

    server->add_key( key, takes_program_audio );
  }

  /* Controller registers itself in EventLoop */
  ServerController controller { server, *loop };

  /* Print out statistics to terminal */
  StatsPrinterTask stats_printer { loop };
  stats_printer.add( server );

  server->initialize_clock();

  const bool include_second_channels = getenv( "STAGECAST_2CH" );

  /* JSON updates */
  UnixDatagramSocket json_updates;
  json_updates.set_blocking( false );
  Address json_update_address { Address::abstract_unix( "stagecast-server-audio-json" ) };
  const uint64_t json_update_interval = 50'000'000;
  uint64_t next_json_update = Timer::timestamp_ns() + json_update_interval;
  Json::Value root;
  ostringstream json_str;
  loop->add_rule(
    "JSON update",
    json_updates,
    Direction::Out,
    [&] {
      root.clear();
      json_str.str( "" );
      json_str.clear();
      server->json_summary( root, include_second_channels );
      json_str << root;
      json_updates.sendto_ignore_errors( json_update_address, json_str.str() );
      next_json_update = Timer::timestamp_ns() + json_update_interval;
    },
    [&] { return Timer::timestamp_ns() > next_json_update; } );

  /* Start audio device and event loop */
  while ( loop->wait_next_event( 0 ) != EventLoop::Result::Exit ) {
  }
}

int main( int argc, char* argv[] )
{
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

  return EXIT_SUCCESS;
}
