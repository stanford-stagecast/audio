#include "clock.hh"
#include "exception.hh"
#include "timestamp.hh"

#include <cmath>

#include "timer.hh"
#include <iostream>

using namespace std;

Clock::Clock( const uint64_t global_ts )
  : global_ts_last_update_( global_ts )
  , local_ts_last_sample_( 0 )
  , synced_( false )
{}

void Clock::reset( const uint64_t global_ts, const uint64_t local_ts_initial_value )
{
  global_ts_last_update_ = global_ts;
  local_ts_last_sample_ = local_ts_initial_value;
  local_clock_ = local_ts_initial_value;
  clock_rate_ = ideal_clock_rate_ = 1.0;
  synced_ = true;
  stats_.resets++;
  stats_.last_reset = global_ts;
  stats_.biggest_gap_since_reset = 0;
  stats_.biggest_diff_since_reset = 0;
  stats_.last_clock_difference = stats_.smoothed_clock_difference = 0;
}

void Clock::time_passes( const uint64_t global_ts )
{
  if ( not synced() ) {
    return;
  }

  if ( global_ts < global_ts_last_update_ ) {
    throw runtime_error( "global clock moved backwards" );
  }

  const uint64_t global_ticks_elapsed = global_ts - global_ts_last_update_;
  global_ts_last_update_ = global_ts;
  local_clock_ += global_ticks_elapsed * clock_rate_;

  if ( local_clock_ >= ( uint64_t { 1 } << numeric_limits<double>::digits ) ) {
    throw runtime_error( "clock reached upper limit" );
  }

  const double gap_magnitude = abs( local_clock_ - local_ts_last_sample_ );

  stats_.biggest_gap_since_reset = max( stats_.biggest_gap_since_reset, gap_magnitude );

  if ( gap_magnitude > MAX_GAP ) {
    synced_ = false;
    stats_.gaps++;
  }
}

inline double cents( const double val )
{
  return 1200.0 * log2( val );
}

void Clock::new_sample( const uint64_t global_ts, const uint64_t local_ts_sample )
{
  const uint64_t local_ts_last_sample_save = local_ts_last_sample_;
  const uint64_t global_ts_last_sample_save = global_ts_last_sample_;

  /* Evolve clock forwards based on previous rate */
  global_ts_last_sample_ = global_ts;
  local_ts_last_sample_ = local_ts_sample;
  time_passes( global_ts );

  if ( not synced() ) {
    reset( global_ts, local_ts_sample );
    return;
  }

  /* Update rate sample */
  const double elapsed_time = global_ts - global_ts_last_sample_save;
  if ( elapsed_time > 0 ) {
    const double instantaneous_rate_sample = ( local_ts_sample - local_ts_last_sample_save ) / elapsed_time;
    const double update_alpha
      = min( 1.0, elapsed_time / ( 48000 * 60 ) ); // two samples one minute apart = full marks
    ideal_clock_rate_ = update_alpha * instantaneous_rate_sample + ( 1 - update_alpha ) * ideal_clock_rate_;
  }

  /* Now update rate */
  stats_.last_clock_difference = local_ts_sample - local_clock_;
  stats_.smoothed_clock_difference
    = ALPHA * stats_.last_clock_difference + ( 1 - ALPHA ) * stats_.smoothed_clock_difference;

  stats_.jitter_squared
    = ALPHA * stats_.last_clock_difference * stats_.last_clock_difference + ( 1 - ALPHA ) * stats_.jitter_squared;

  const double abs_clock_difference = abs( stats_.last_clock_difference );
  stats_.biggest_diff_since_reset = max( stats_.biggest_diff_since_reset, abs_clock_difference );

  if ( abs_clock_difference > MAX_SKEW ) {
    /* avulsion */

    stats_.avulsions++;
    reset( global_ts, local_ts_sample );
    return;
  }

  /* adjust rate to narrow difference */
  clock_rate_ = ideal_clock_rate_ + stats_.smoothed_clock_difference / INTERCEPT_HORIZON;

  cout << global_ts << " " << local_ts_sample << " " << stats_.last_clock_difference << " "
       << stats_.smoothed_clock_difference << " " << 1000 * cents( clock_rate_ ) << " "
       << 1000 * cents( ideal_clock_rate_ ) << " " << sqrt( stats_.jitter_squared ) << " "
       << ( local_ts_sample - local_ts_last_sample_save ) / elapsed_time << "\n";
}

void Clock::summary( ostream& out ) const
{
  out << "Clock " << ( synced() ? "synced" : "unsynced" );
  out << " resets=" << stats_.resets << " (" << stats_.gaps << " gaps, " << stats_.avulsions << " avulsions";
  if ( stats_.resets ) {
    out << ", last reset at ";
    pp_samples( out, stats_.last_reset );
    out << ", biggest gap=" << fixed << setprecision( 1 ) << stats_.biggest_gap_since_reset;
  }
  out << ")";
  if ( synced() ) {
    out << ", rate=" << fixed << setprecision( 2 ) << 1000 * cents( rate() ) << " millicents";
    out << ", ideal=" << fixed << setprecision( 2 ) << 1000 * cents( ideal_clock_rate_ ) << " millicents";
    out << ", jitter=" << fixed << setprecision( 2 ) << sqrt( stats_.jitter_squared );

    out << ", diff: cur=" << fixed << setprecision( 1 ) << stats_.last_clock_difference;
    out << ", smoothed=" << fixed << setprecision( 1 ) << stats_.smoothed_clock_difference;
    out << ", biggest=" << fixed << setprecision( 1 ) << stats_.biggest_diff_since_reset;
  }
  out << "\n";
}
