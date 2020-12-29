#include "audio_task.hh"
#include "timestamp.hh"

using namespace std;
using namespace chrono;

AudioDeviceTask::AudioDeviceTask( const string_view interface_name, EventLoop& loop )
  : device_( interface_name )
{
  device_.initialize();

  install_rules( loop );
}

void AudioDeviceTask::install_rules( EventLoop& loop )
{
  loop.add_rule(
    "audio loopback [fast path]", [&] { service_device(); }, [&] { return device_.mic_has_samples(); } );

  loop.add_rule(
    "audio loopback [slow path]",
    device_.fd(),
    Direction::In,
    [&] { service_device(); },
    [] { return true; },
    [] {},
    [&] {
      device_.recover();
      return true;
    } );
}

void AudioDeviceTask::service_device()
{
  device_.loopback( capture_, playback_ );
  playback_.pop( device_.cursor() - playback_.range_begin() );
}

void AudioDeviceTask::summary( ostream& out ) const
{
  if ( device_.statistics().sample_stats.samples_counted ) {
    out << "Audio info: dB = [ " << setw( 3 ) << setprecision( 1 ) << fixed
        << float_to_dbfs(
             sqrt( device_.statistics().sample_stats.ssa_ch1 / device_.statistics().sample_stats.samples_counted ) )
        << "/" << setw( 3 ) << setprecision( 1 ) << fixed
        << float_to_dbfs( device_.statistics().sample_stats.max_ch1_amplitude ) << ", ";

    out << setw( 3 ) << setprecision( 1 ) << fixed
        << float_to_dbfs(
             sqrt( device_.statistics().sample_stats.ssa_ch2 / device_.statistics().sample_stats.samples_counted ) )
        << "/" << setw( 3 ) << setprecision( 1 ) << fixed
        << float_to_dbfs( device_.statistics().sample_stats.max_ch2_amplitude ) << " ]";
  }

  out << " cursor=";
  pp_samples( out, device_.cursor() );
  if ( device_.cursor() - capture_.range_begin() > 120 ) {
    out << " capture=";
    pp_samples( out, device_.cursor() - capture_.range_begin() );
  }
  if ( device_.cursor() != playback_.range_begin() ) {
    out << " playback=";
    pp_samples( out, device_.cursor() - playback_.range_begin() );
  }

  if ( device_.statistics().recoveries ) {
    out << " recoveries=" << device_.statistics().recoveries;
  }

  if ( device_.statistics().last_recovery
       and ( device_.cursor() - device_.statistics().last_recovery < 48000 * 60 ) ) {
    out << " last recovery=";
    pp_samples( out, device_.cursor() - device_.statistics().last_recovery );
    out << " skipped=" << device_.statistics().sample_stats.samples_skipped;
  }

  if ( device_.statistics().max_microphone_avail > 32 ) {
    out << " mic<=" << device_.statistics().max_microphone_avail << "!";
  }
  if ( device_.statistics().min_headphone_delay <= 6 ) {
    out << " phone>=" << device_.statistics().min_headphone_delay << "!";
  }
  if ( device_.statistics().max_combined_samples > 64 ) {
    out << " combined<=" << device_.statistics().max_combined_samples << "!";
  }
  if ( device_.statistics().empty_wakeups ) {
    out << " empty=" << device_.statistics().empty_wakeups << "/" << device_.statistics().total_wakeups << "!";
  }

  out << "\n";
}
