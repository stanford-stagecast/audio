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

inline void pp_samples( ostringstream& out, const int64_t sample_count )
{
  int64_t s = sample_count;
  bool negative = false;

  if ( s < 0 ) {
    out << "-{";
    negative = true;
    s = -s;
  }

  const size_t minutes = s / ( 48000 * 60 );
  s = s % ( 48000 * 60 );

  const size_t seconds = s / 48000;
  s = s % 48000;

  const size_t ms = s / 48;
  s = s % 48;

  if ( minutes ) {
    out << setw( 2 ) << setfill( '0' ) << minutes << "m";
  }

  out << setw( 2 ) << setfill( '0' ) << seconds << "s";
  out << setw( 3 ) << setfill( '0' ) << ms;
  if ( s ) {
    out << "+" << setw( 2 ) << setfill( '0' ) << s;
  } else {
    out << "   ";
  }

  if ( negative ) {
    out << "}";
  }
}

void program_body()
{
  ios::sync_with_stdio( false );

  AudioBuffer audio_capture { 65536 }, audio_playback { 65536 };

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
    "audio loopback [fast path]",
    [&] {
      uac2.loopback( audio_capture, audio_playback );
      audio_playback.pop( uac2.cursor() - audio_playback.range_begin() );
    },
    [&] { return uac2.mic_has_samples(); } );

  loop.add_rule(
    "audio loopback [slow path]",
    uac2.fd(),
    Direction::In,
    [&] {
      uac2.loopback( audio_capture, audio_playback );
      audio_playback.pop( uac2.cursor() - audio_playback.range_begin() );
    },
    [] { return true; },
    [] {},
    [&] {
      uac2.recover();
      return true;
    } );

  OpusEncoder encoder1 { 128000, 48000 }, encoder2 { 128000, 48000 };
  opus_frame frame1, frame2;
  size_t enc1_encode_cursor {}, enc2_encode_cursor {};

  loop.add_rule(
    "encode [ch1]",
    [&] {
      encoder1.encode( audio_capture.ch1().region( enc1_encode_cursor, 120 ), frame1 );
      enc1_encode_cursor += 120;
    },
    [&] { return uac2.cursor() >= enc1_encode_cursor + 120; } );

  loop.add_rule(
    "encode [ch2]",
    [&] {
      encoder2.encode( audio_capture.ch2().region( enc2_encode_cursor, 120 ), frame2 );
      enc2_encode_cursor += 120;
      const size_t min_encode_cursor = min( enc1_encode_cursor, enc2_encode_cursor );
      if ( min_encode_cursor > audio_capture.range_begin() ) {
        audio_capture.pop( min_encode_cursor - audio_capture.range_begin() );
      }
    },
    [&] { return uac2.cursor() >= enc2_encode_cursor + 120; } );

  auto next_stats_print = steady_clock::now();
  const auto stats_interval = milliseconds( 500 );

  auto next_stats_reset = steady_clock::now();
  const auto stats_reset_interval = seconds( 10 );

  ostringstream update;

  loop.add_rule(
    "generate statistics",
    [&] {
      if ( uac2.statistics().sample_stats.samples_counted ) {
        update << "dB = [ " << setw( 3 ) << setprecision( 1 ) << fixed
               << float_to_dbfs( sqrt( uac2.statistics().sample_stats.ssa_ch1
                                       / uac2.statistics().sample_stats.samples_counted ) )
               << "/" << setw( 3 ) << setprecision( 1 ) << fixed
               << float_to_dbfs( uac2.statistics().sample_stats.max_ch1_amplitude ) << ", ";

        update << setw( 3 ) << setprecision( 1 ) << fixed
               << float_to_dbfs( sqrt( uac2.statistics().sample_stats.ssa_ch2
                                       / uac2.statistics().sample_stats.samples_counted ) )
               << "/" << setw( 3 ) << setprecision( 1 ) << fixed
               << float_to_dbfs( uac2.statistics().sample_stats.max_ch2_amplitude ) << " ]";
      }

      update << " cursor=";
      pp_samples( update, uac2.cursor() );
      if ( uac2.cursor() - audio_capture.range_begin() > 120 ) {
        update << " capture=";
        pp_samples( update, uac2.cursor() - audio_capture.range_begin() );
      }
      if ( uac2.cursor() != audio_playback.range_begin() ) {
        update << " playback=";
        pp_samples( update, uac2.cursor() - audio_playback.range_begin() );
      }

      if ( uac2.statistics().last_recovery and ( uac2.cursor() - uac2.statistics().last_recovery < 48000 * 60 ) ) {
        update << " last recovery=";
        pp_samples( update, uac2.cursor() - uac2.statistics().last_recovery );
        update << " recoveries=" << uac2.statistics().recoveries;
        update << " skipped=" << uac2.statistics().sample_stats.samples_skipped;
      }

      if ( uac2.statistics().max_microphone_avail > 32 ) {
        update << " mic<=" << uac2.statistics().max_microphone_avail << "!";
      }
      if ( uac2.statistics().min_headphone_delay <= 6 ) {
        update << " phone>=" << uac2.statistics().min_headphone_delay << "!";
      }
      if ( uac2.statistics().max_combined_samples > 64 ) {
        update << " combined<=" << uac2.statistics().max_combined_samples << "!";
      }
      if ( uac2.statistics().empty_wakeups ) {
        update << " empty=" << uac2.statistics().empty_wakeups << "/" << uac2.statistics().total_wakeups << "!";
      }

      update << "\n";
      uac2.reset_statistics();

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
      update.str( {} );
      update.clear();
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
