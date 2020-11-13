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
#include "opus.hh"
#include "typed_ring_buffer.hh"

using namespace std;
using namespace std::chrono;

void program_body()
{
  ios::sync_with_stdio( false );

  AudioBuffer audio_capture { 65536 }, audio_playback { 65536 };
  size_t capture_index {}, playback_index {};

  const auto [name, interface_name] = ALSADevices::find_device( "UAC-2, USB Audio" );
  const auto device_claim = AudioDeviceClaim::try_claim( name );
  AudioPair uac2 { interface_name };
  uac2.initialize();

  EventLoop loop;

  FileDescriptor input { CheckSystemCall( "dup STDIN_FILENO", dup( STDIN_FILENO ) ) };
  FileDescriptor output { CheckSystemCall( "dup STDOUT_FILENO", dup( STDOUT_FILENO ) ) };
  input.set_blocking( false );
  output.set_blocking( false );

  RingBuffer output_rb { 65536 };

  loop.add_rule(
    "audio loopback",
    uac2.fd(),
    Direction::In,
    [&] { uac2.loopback( audio_capture, capture_index, audio_playback, playback_index ); },
    [] { return true; },
    [] {},
    [&] {
      uac2.recover();
      return true;
    } );

  OpusEncoder encoder1 { 128000, 48000 }, encoder2 { 128000, 48000 };
  opus_frame frame1, frame2;

  loop.add_rule(
    "encode",
    [&] {
      encoder1.encode( audio_capture.ch1().region( audio_capture.range_begin(), 120 ), frame1 );
      encoder2.encode( audio_capture.ch2().region( audio_capture.range_begin(), 120 ), frame2 );
      audio_capture.pop( 120 );
    },
    [&] { return capture_index >= audio_capture.range_begin() + 120; } );

  auto next_stats_print = steady_clock::now();
  const auto stats_interval = milliseconds( 500 );

  ostringstream update;

  loop.add_rule(
    "generate statistics",
    [&] {
      update << "peak dBFS=[ " << float_to_dbfs( uac2.statistics().sample_stats.max_ch1_amplitude ) << ", "
             << float_to_dbfs( uac2.statistics().sample_stats.max_ch2_amplitude ) << " ]";
      update << " capture range= " << audio_capture.range_begin();
      update << " playback range= " << audio_playback.range_begin();
      update << " capture=" << capture_index << " playback=" << playback_index;
      update << " recov=" << uac2.statistics().recoveries;
      update << " skipped=" << uac2.statistics().sample_stats.samples_skipped;
      update << " empty=" << uac2.statistics().empty_wakeups << "/" << uac2.statistics().total_wakeups;
      update << " mic<=" << uac2.statistics().max_microphone_avail;
      update << " phone>=" << uac2.statistics().min_headphone_delay;
      update << " comb<=" << uac2.statistics().max_combined_samples;

      update << "\n";

      update << loop.summary() << "\n";
      update << global_timer().summary() << endl;

      const auto str = update.str();
      if ( output_rb.writable_region().size() >= str.size() ) {
        output_rb.push_from_const_str( str );
      }
      update.clear();

      uac2.reset_statistics();
      loop.reset_statistics();
      next_stats_print = steady_clock::now() + stats_interval;
    },
    [&] { return steady_clock::now() > next_stats_print; } );

  loop.add_rule(
    "print statistics",
    output,
    Direction::Out,
    [&] { output_rb.pop_to_fd( output ); },
    [&] { return output_rb.bytes_stored() > 0; } );

  uac2.start();
  while ( loop.wait_next_event( -1 ) != EventLoop::Result::Exit ) {
  }

  cout << loop.summary() << "\n";
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
