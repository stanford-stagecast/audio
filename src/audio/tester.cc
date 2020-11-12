#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <thread>

#include "alsa_devices.hh"
#include "audio_device_claim.hh"
#include "eventloop.hh"
#include "exception.hh"
#include "typed_ring_buffer.hh"

using namespace std;
using namespace std::chrono;

void program_body()
{
  ios::sync_with_stdio( false );

  AudioBuffer audio_capture { 65536 }, audio_playback { 65536 };
  size_t capture_index {}, playback_index {};
  int playback_index_offset {};

  size_t capture_read_index {}, playback_write_index {};

  const auto [name, interface_name] = ALSADevices::find_device( "UAC-2, USB Audio" );
  const auto device_claim = AudioDeviceClaim::try_claim( name );
  AudioPair uac2 { interface_name };
  uac2.initialize();

  EventLoop loop;

  FileDescriptor input { CheckSystemCall( "dup STDIN_FILENO", dup( STDIN_FILENO ) ) };

  const auto jitter_calculation_interval = milliseconds( 50 );

  auto next_jitter_calculation = steady_clock::now() + jitter_calculation_interval;

  for ( int i = -300; i < 300; i++ ) {
    audio_playback.jitter_buffer().add_sample_point( i * 48 );
  }

  loop.add_rule(
    "calculate jitter",
    [&] {
      audio_playback.jitter_buffer().sample( playback_index, playback_index_offset );
      next_jitter_calculation += jitter_calculation_interval;

      const auto best_offset = audio_playback.jitter_buffer().best_offset( 0.98 );
      if ( best_offset.has_value() ) {
        if ( best_offset.value() > int64_t( playback_index - capture_index ) + 23 ) {
          playback_index++;
          playback_index_offset++;
        }

        if ( best_offset.value() < int64_t( playback_index - capture_index ) ) {
          playback_index--;
          playback_index_offset--;
        }
      }
    },
    [&] { return steady_clock::now() >= next_jitter_calculation; } );

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

  string keyboard;
  keyboard.resize( 256 );

  auto next_stats_print = steady_clock::now();
  const auto stats_interval = milliseconds( 250 );

  loop.add_rule( "adjust playback index", input, Direction::In, [&] {
    input.read( static_cast<string_view>( keyboard ) );
    if ( not keyboard.empty() and keyboard.front() == '+' and playback_index > 48 ) {
      playback_index -= 48;
      playback_index_offset -= 48;
    }
    if ( not keyboard.empty() and keyboard.front() == '-' ) {
      playback_index += 48;
      playback_index_offset += 48;
    }

    next_stats_print = steady_clock::now();
  } );

  loop.add_rule(
    "copy capture-> playback",
    [&] {
      while ( capture_read_index < capture_index ) {
        audio_playback.safe_set( playback_write_index++, audio_capture.safe_get( capture_read_index++ ) );
      }
      audio_capture.pop( capture_index - audio_capture.range_begin() );
    },
    [&] { return capture_read_index < capture_index; } );

  loop.add_rule(
    "pop from playback",
    [&] { audio_playback.pop( playback_index - audio_playback.range_begin() - 48000 ); },
    [&] { return playback_index > audio_playback.range_begin() + 48000; } );

  loop.add_rule(
    "print statistics",
    [&] {
      cout << "peak dBFS=[ " << float_to_dbfs( uac2.statistics().sample_stats.max_ch1_amplitude ) << ", "
           << float_to_dbfs( uac2.statistics().sample_stats.max_ch2_amplitude ) << " ]";
      cout << " capture=" << capture_index / 48000.0 << " playback=" << playback_index / 48000.0;
      cout << " recov=" << uac2.statistics().recoveries;
      cout << " skipped=" << uac2.statistics().sample_stats.samples_skipped;
      cout << " empty=" << uac2.statistics().empty_wakeups << "/" << uac2.statistics().total_wakeups;
      cout << " mic<=" << uac2.statistics().max_microphone_avail;
      cout << " phone>=" << uac2.statistics().min_headphone_delay;
      cout << " comb<=" << uac2.statistics().max_combined_samples;
      cout << " best_offset=" << -audio_playback.jitter_buffer().best_offset( 0.98 ).value_or( -1 );
      cout << " offset= " << int64_t( capture_index - playback_index );
      cout << " quality=" << setw( 4 ) << fixed << setprecision( 1 )
           << lrint( 1000 * uac2.statistics().sample_stats.playability ) / 10.0;
      cout << "\n";

      cout << loop.summary() << "\n";
      cout << global_timer().summary() << endl;
      uac2.reset_statistics();
      loop.reset_statistics();
      next_stats_print = steady_clock::now() + stats_interval;
    },
    [&] { return steady_clock::now() > next_stats_print; } );

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
