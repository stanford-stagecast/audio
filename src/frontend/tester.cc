#include <cstdlib>
#include <iostream>

#include "alsa_devices.hh"
#include "audio_device_claim.hh"
#include "eventloop.hh"

#include "audio_task.hh"
#include "encoder_task.hh"
#include "stats_printer.hh"

#include "formats.hh"

using namespace std;

void program_body()
{
  ios::sync_with_stdio( false );

  auto loop = make_shared<EventLoop>();

  /* Audio task gets first priority in EventLoop */
  const auto [name, interface_name] = ALSADevices::find_device( "UAC-2, USB Audio" );
  const auto device_claim = AudioDeviceClaim::try_claim( name );
  auto uac2 = make_shared<AudioDeviceTask>( interface_name, *loop );

  /* Opus encoder task registers itself in EventLoop */
  OpusEncoderTask encoder { 128000, 48000, uac2, *loop };

  /* We should transmit the frames over the Internet, but ignore them for now */
  string s;
  loop->add_rule(
    "transmit Opus frames",
    [&] {
      {
        Packet packet;
        packet.num_frames = 8;
        for ( uint8_t i = 0; i < 8; i++ ) {
          packet.frames.at( i ) = { uint32_t( encoder.frame_index() ), encoder.front_ch1(), encoder.front_ch2() };
        }
        s.resize( packet.serialized_length() );
        packet.serialize( string_span::from_view( s ) );
        encoder.pop_frame();
      }

      Packet packet2;
      if ( 0 == packet2.parse( s ) ) {
        cerr << "audio parse error\n";
      }
      if ( 8 != packet2.num_frames ) {
        cerr << "another error\n";
      }
    },
    [&] { return encoder.has_frame(); } );

  /* Print out statistics to terminal */
  StatsPrinterTask stats_printer { uac2, loop };

  /* Start audio device and event loop */
  uac2->device().start();
  while ( loop->wait_next_event( -1 ) != EventLoop::Result::Exit ) {
  }
}

int main()
{
  try {
    program_body();
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << "\n";
    cerr << global_timer().summary() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
