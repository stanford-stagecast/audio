#include <cstdlib>
#include <iostream>

#include "alsa_devices.hh"
#include "audio_device_claim.hh"
#include "eventloop.hh"

#include "audio_task.hh"
#include "encoder_task.hh"
#include "stats_printer.hh"

#include "connection.hh"

using namespace std;

void program_body( const string& host, const string& service, const string& send_key, const string& recv_key )
{
  ios::sync_with_stdio( false );

  auto loop = make_shared<EventLoop>();

  /* Audio task gets first priority in EventLoop */
  const auto [name, interface_name] = ALSADevices::find_device( "UAC-2, USB Audio" );
  const auto device_claim = AudioDeviceClaim::try_claim( name );
  auto uac2 = make_shared<AudioDeviceTask>( interface_name, *loop );

  /* Opus encoder task registers itself in EventLoop */
  auto encoder = make_shared<ClientEncoderTask>( 128000, 48000, uac2, *loop );

  /* Network client registers itself in EventLoop */
  const Address stagecast_server { host, service };
  auto network_client = make_shared<NetworkClient>(
    stagecast_server, Base64Key { send_key }, Base64Key { recv_key }, encoder, *loop );

  /* Print out statistics to terminal */
  StatsPrinterTask stats_printer { uac2, { network_client }, loop };

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

    if ( argc != 5 ) {
      cerr << "Usage: " << argv[0] << " HOST SERVICE SEND_KEY RECV_KEY\n";
      return EXIT_FAILURE;
    }

    program_body( argv[1], argv[2], argv[3], argv[4] );
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << "\n";
    global_timer().summary( cerr );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
