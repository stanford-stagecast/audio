#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <thread>

#include "alsa_devices.hh"
#include "eventloop.hh"
#include "exception.hh"
#include "opus.hh"
#include "socket.hh"
#include "typed_ring_buffer.hh"

#include "network.hh"

using namespace std;
using namespace std::chrono;

void program_body( const string host, const string port )
{
  ios::sync_with_stdio( false );

  AudioBuffer audio_capture { 65536 }, audio_playback { 65536 };
  size_t capture_index {}, playback_index {};
  size_t capture_read_index {}, playback_write_index {};
  int playback_offset = 0;

  UDPSocket sock;
  sock.set_blocking( false );
  Address server { host, port };

  const auto [name, interface_name] = ALSADevices::find_device( "UAC-2, USB Audio" );
  AudioPair uac2 { interface_name };
  uac2.initialize();

  OpusEncoder opusenc1 { 128000, 48000 }, opusenc2 { 128000, 48000 };
  OpusDecoder opusdec1 { 48000 }, opusdec2 { 48000 };

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
      audio_playback.jitter_buffer().sample( playback_index, playback_offset );
      next_jitter_calculation += jitter_calculation_interval;

      const auto best_offset = audio_playback.jitter_buffer().best_offset( 0.98 );
      if ( best_offset.has_value() ) {
        if ( best_offset.value() - 48 < int64_t( playback_index - capture_index ) and playback_index >= 24 ) {
          // fix underflows aggressively
          playback_index -= 24;
          playback_offset -= 24;
        }

        if ( best_offset.value() - 48 > int64_t( playback_index - capture_index ) ) {
          playback_index++;
          playback_offset++;
        }
      }
    },
    [&] { return steady_clock::now() >= next_jitter_calculation; } );

  loop.add_rule(
    "audio loopback",
    uac2.fd(),
    Direction::In,
    [&] {
      uac2.loopback( audio_capture, capture_index, audio_playback, playback_index );

      playback_write_index = max( playback_write_index, playback_index );
    },
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
    }
    if ( not keyboard.empty() and keyboard.front() == '-' ) {
      playback_index += 48;
    }

    next_stats_print = steady_clock::now();
  } );

  Packet pack;

  loop.add_rule(
    "compress and send",
    sock,
    Direction::Out,
    [&] {
      pack.size_1 = opusenc1.encode( audio_capture.ch1().region( capture_read_index, 120 ), pack.f1() );
      pack.size_2 = opusenc1.encode( audio_capture.ch2().region( capture_read_index, 120 ), pack.f2() );

      sock.sendto( server, pack );

      capture_read_index += 120;
      //      playback_write_index += 120;
      audio_capture.pop( 120 );
    },
    [&] { return capture_index >= capture_read_index + 120; } );

  UDPSocket::received_datagram dg_in { { "0", "0" }, {} };

  loop.add_rule( "receive and decompress", sock, Direction::In, [&] {
    sock.recv( dg_in );
    if ( dg_in.payload.size() != sizeof( Packet ) ) {
      return;
    }

    memcpy( &pack, dg_in.payload.data(), sizeof( Packet ) );

    const size_t samples_written1
      = opusdec1.decode( pack.f1(), audio_playback.ch1().region( playback_write_index, 120 ) );
    const size_t samples_written2
      = opusdec2.decode( pack.f2(), audio_playback.ch2().region( playback_write_index, 120 ) );

    if ( samples_written1 != 120 or samples_written2 != 120 ) {
      throw runtime_error( "bad opus frame" );
    }

    audio_playback.touch( playback_write_index, 120 );
    playback_write_index += 120;
  } );

  loop.add_rule(
    "pop from playback",
    [&] { audio_playback.pop( playback_index - audio_playback.range_begin() - 48000 ); },
    [&] { return playback_index > audio_playback.range_begin() + 48000; } );

  loop.add_rule(
    "print statistics",
    [&] {
      cout << "peak dBFS=[ " << float_to_dbfs( uac2.statistics().sample_stats.max_ch1_amplitude ) << ", "
           << float_to_dbfs( uac2.statistics().sample_stats.max_ch2_amplitude ) << " ]";
      cout << " capture range= " << audio_capture.range_begin();
      cout << " playback range= " << audio_playback.range_begin();
      cout << " capture=" << capture_index << " playback=" << playback_index;
      cout << " capture-playback=" << capture_index - playback_index << "\n";
      cout << " recov=" << uac2.statistics().recoveries;
      cout << " skipped=" << uac2.statistics().sample_stats.samples_skipped;
      cout << " empty=" << uac2.statistics().empty_wakeups << "/" << uac2.statistics().total_wakeups;
      cout << " mic<=" << uac2.statistics().max_microphone_avail;
      cout << " phone>=" << uac2.statistics().min_headphone_delay;
      cout << " comb<=" << uac2.statistics().max_combined_samples;

      auto off = audio_playback.jitter_buffer().best_offset( 0.98 );
      if ( off.has_value() ) {
        cout << " best_offset=" << off.value();
      } else {
        cout << " BAD";
      }
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

int main( int argc, char* argv[] )
{
  try {
    if ( argc != 3 ) {
      cerr << "Usage.\n";
      return EXIT_FAILURE;
    }

    program_body( argv[1], argv[2] );
    cout << global_timer().summary() << "\n";
  } catch ( const exception& e ) {
    cout << "Exception: " << e.what() << "\n";
    cout << global_timer().summary() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
