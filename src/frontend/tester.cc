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
        packet.frames.length = 8;
        for ( uint8_t i = 0; i < 8; i++ ) {
          packet.frames.elements.at( i )
            = { uint32_t( encoder.frame_index() ), encoder.front_ch1(), encoder.front_ch2() };
        }
        s.resize( packet.serialized_length() );
        Serializer serializer { string_span::from_view( s ) };
        packet.serialize( serializer );
      }

      Packet packet2;
      Parser parser { s };
      parser.object( packet2 );
      if ( parser.error() ) {
        cerr << "parse error\n";
      }

      if ( packet2.frames.length != 8 ) {
        abort();
      }

      if ( packet2.frames.elements.at( 2 ).frame_index != encoder.frame_index() ) {
        abort();
      }

      if ( packet2.frames.elements.at( 2 ).ch1.as_string_view() != encoder.front_ch1().as_string_view() ) {
        abort();
      }

      encoder.pop_frame();
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
