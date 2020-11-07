#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

#include "alsa_devices.hh"
#include "eventloop.hh"
#include "exception.hh"
#include "typed_ring_buffer.hh"

#ifndef NDBUS
#include "device_claim_util.hh"
#endif

using namespace std;
using namespace std::chrono;

void program_body()
{
  ios::sync_with_stdio( false );

  AudioBuffer audio_output { 65536 }, audio_input { 65536 };

  AudioPair uac2 = claim_uac2();
  uac2.initialize();

  EventLoop loop;

  FileDescriptor input { CheckSystemCall( "dup STDIN_FILENO", dup( STDIN_FILENO ) ) };

  auto loopback_rule = loop.add_rule(
    "audio loopback",
    uac2.fd(),
    Direction::In,
    [&] { uac2.loopback( audio_output ); },
    [] { return true; },
    [] {},
    [&] {
      uac2.recover();
      return true;
    } );

  loop.add_rule( "exit on keystroke", input, Direction::In, [&] {
    loopback_rule.cancel();
    input.close();
  } );

  loop.add_rule(
    "read from buffer",
    [&] {
      audio_output.ch1.pop( audio_output.ch1.num_stored() );
      audio_output.ch2.pop( audio_output.ch2.num_stored() );
    },
    [&] { return audio_output.ch1.num_stored() > 0; } );

  auto next_stats_print = steady_clock::now() + seconds( 3 );
  loop.add_rule(
    "print statistics",
    [&] {
      cout << "peak dBFS=[ " << float_to_dbfs( uac2.statistics().sample_stats.max_ch1_amplitude ) << ", "
           << float_to_dbfs( uac2.statistics().sample_stats.max_ch2_amplitude ) << " ]";
      cout << " buffer size=" << audio_output.ch1.capacity() - audio_output.ch1.num_stored();
      cout << " recov=" << uac2.statistics().recoveries;
      cout << " skipped=" << uac2.statistics().sample_stats.samples_skipped;
      cout << " range=[" << audio_output.ch1.range_begin() / 48000.0 << ".."
           << audio_output.ch1.range_end() / 48000.0 << "]";
      cout << " empty=" << uac2.statistics().empty_wakeups << "/" << uac2.statistics().total_wakeups;
      cout << " mic<=" << uac2.statistics().max_microphone_avail;
      cout << " phone>=" << uac2.statistics().min_headphone_delay;
      cout << " comb<=" << uac2.statistics().max_combined_samples;
      cout << "\n";
      cout << loop.summary() << "\n";
      cout << global_timer().summary() << endl;
      uac2.reset_statistics();
      next_stats_print = steady_clock::now() + seconds( 3 );
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
