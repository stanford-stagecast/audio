#include <cstdlib>
#include <iostream>

#include <sched.h>

#include "alsa_devices.hh"
#include "audio_task.hh"
#include "control_messages.hh"
#include "controller.hh"
#include "encoder_task.hh"
#include "eventloop.hh"
#include "keys.hh"
#include "networkclient.hh"
#include "stats_printer.hh"

#ifndef NDBUS
#include "audio_device_claim.hh"
#endif /* NDBUS */

using namespace std;

void program_body( const string& host, const string& service, const string& key_filename )
{
  ios::sync_with_stdio( false );

  /* real-time priority */
  try {
    sched_param param;
    param.sched_priority = CheckSystemCall( "sched_get_priority_max", SCHED_FIFO );
    CheckSystemCall( "sched_setscheduler", sched_setscheduler( 0, SCHED_FIFO | SCHED_RESET_ON_FORK, &param ) );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
  }

  /* read key */
  ReadOnlyFile keyfile { key_filename };
  Parser p { keyfile };
  LongLivedKey key { p };

  cerr << "Starting client as " << key.name() << ".\n";

  auto loop = make_shared<EventLoop>();

  /* Audio task gets first priority in EventLoop */
  const auto [name, interface_name] = ALSADevices::find_device( { "Scarlett", "UAC-2, USB Audio" } );

#ifndef NDBUS
  const auto device_claim = AudioDeviceClaim::try_claim( name );
#endif /* NDBUS */

  auto uac2 = make_shared<AudioDeviceTask>( interface_name, *loop );

  const bool send_second_channel = getenv( "STAGECAST_2CH" );

  /* Opus encoder task registers itself in EventLoop */
  auto encoder = make_shared<ClientEncoderTask>( 96000, ( send_second_channel ? 96000 : 600 ), 48000, uac2, *loop );

  /* Network client registers itself in EventLoop */
  const Address stagecast_server { host, service };
  auto network_client = make_shared<NetworkClient>( stagecast_server, key, encoder, uac2, *loop );

  /* Controller registers itself in EventLoop */
  ClientController controller { network_client, uac2, *loop };

  /* Print out statistics to terminal */
  StatsPrinterTask stats_printer { loop };
  stats_printer.add( uac2 );
  stats_printer.add( network_client );

  /* Send statistics to server */
  const uint64_t update_interval = 50'000'000;
  uint64_t next_update = Timer::timestamp_ns() + update_interval;
  NetString update_str;
  loop->add_rule(
    "stats update",
    [&] {
      client_report report;
      report.resets = network_client->cursor().stats().resets;
      report.target_lag
        = min( uint32_t( numeric_limits<uint16_t>::max() ), network_client->cursor().target_lag_samples() );
      report.min_lag
        = min( uint32_t( numeric_limits<uint16_t>::max() ), network_client->cursor().min_lag_samples() );
      report.max_lag
        = min( uint32_t( numeric_limits<uint16_t>::max() ), network_client->cursor().max_lag_samples() );
      report.actual_lag = network_client->cursor().stats().mean_margin_to_frontier;
      report.quality = network_client->cursor().stats().quality;
      report.self_gain = uac2->loopback_gain();

      Serializer s { update_str.mutable_buffer() };
      s.object( report );
      update_str.resize( s.bytes_written() );

      network_client->queue_update( update_str );
      next_update = Timer::timestamp_ns() + update_interval;
    },
    [&] { return network_client->has_session() and ( Timer::timestamp_ns() > next_update ); } );

  /* Start audio device and event loop */
  uac2->device().start();
  while ( loop->wait_next_event( -1 ) != EventLoop::Result::Exit ) {
  }
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }

    if ( argc != 4 ) {
      cerr << "Usage: " << argv[0] << " host service keyfile\n";
      return EXIT_FAILURE;
    }

    program_body( argv[1], argv[2], argv[3] );
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << "\n";
    global_timer().summary( cerr );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
