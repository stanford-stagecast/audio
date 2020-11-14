#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

#include "alsa_devices.hh"
#include "audio_device_claim.hh"
#include "audio_task.hh"
#include "eventloop.hh"
#include "exception.hh"
#include "opus.hh"

using namespace std;
using namespace std::chrono;

void program_body()
{
  ios::sync_with_stdio( false );

  EventLoop loop;

  const auto [name, interface_name] = ALSADevices::find_device( "UAC-2, USB Audio" );
  const auto device_claim = AudioDeviceClaim::try_claim( name );

  AudioDeviceTask uac2 { interface_name, loop };

  FileDescriptor output { CheckSystemCall( "dup STDOUT_FILENO", dup( STDOUT_FILENO ) ) };
  output.set_blocking( false );

  RingBuffer output_rb { 65536 };

  OpusEncoder encoder1 { 128000, 48000 }, encoder2 { 128000, 48000 };
  opus_frame frame1, frame2;
  size_t enc1_encode_cursor {}, enc2_encode_cursor {};

  loop.add_rule(
    "encode [ch1]",
    [&] {
      encoder1.encode( uac2.capture().ch1().region( enc1_encode_cursor, 120 ), frame1 );
      enc1_encode_cursor += 120;
    },
    [&] { return uac2.device().cursor() >= enc1_encode_cursor + 120; } );

  loop.add_rule(
    "encode [ch2]",
    [&] {
      encoder2.encode( uac2.capture().ch2().region( enc2_encode_cursor, 120 ), frame2 );
      enc2_encode_cursor += 120;
      const size_t min_encode_cursor = min( enc1_encode_cursor, enc2_encode_cursor );
      if ( min_encode_cursor > uac2.capture().range_begin() ) {
        uac2.capture().pop( min_encode_cursor - uac2.capture().range_begin() );
      }
    },
    [&] { return uac2.device().cursor() >= enc2_encode_cursor + 120; } );

  auto next_stats_print = steady_clock::now();
  const auto stats_interval = milliseconds( 500 );

  auto next_stats_reset = steady_clock::now();
  const auto stats_reset_interval = seconds( 10 );

  ostringstream update;

  loop.add_rule(
    "generate+print statistics",
    [&] {
      update.str( {} );
      update.clear();

      uac2.generate_statistics( update );
      loop.summary( update );
      update << "\n";

      const auto now = steady_clock::now();
      next_stats_print = now + stats_interval;
      if ( now > next_stats_reset ) {
        loop.reset_statistics();
        next_stats_reset = now + stats_reset_interval;
      }

      global_timer().summary( update );
      update << "\n";

      const auto& str = update.str();
      if ( output_rb.writable_region().size() >= str.size() ) {
        output_rb.push_from_const_str( str );
      }
      output_rb.pop_to_fd( output );
    },
    [&] { return steady_clock::now() > next_stats_print; } );

  loop.add_rule(
    "print statistics",
    output,
    Direction::Out,
    [&] { output_rb.pop_to_fd( output ); },
    [&] { return output_rb.bytes_stored() > 0; } );

  uac2.device().start();
  while ( loop.wait_next_event( -1 ) != EventLoop::Result::Exit ) {
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
