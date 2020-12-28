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
  offset_ = global_ts - local_ts_initial_value;
  clock_rate_ = 1.0;
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

void Clock::new_sample( const uint64_t global_ts, const uint64_t local_ts_sample )
{
  local_ts_last_sample_ = local_ts_sample;
  time_passes( global_ts );

  if ( not synced() ) {
    reset( global_ts, local_ts_sample );
    return;
  }

  stats_.last_clock_difference = local_ts_sample - local_clock_;
  stats_.smoothed_clock_difference
    = CLOCK_SLEW_ALPHA * stats_.last_clock_difference + ( 1 - CLOCK_SLEW_ALPHA ) * stats_.smoothed_clock_difference;

  const double abs_clock_difference = abs( stats_.last_clock_difference );
  stats_.biggest_diff_since_reset = max( stats_.biggest_diff_since_reset, abs_clock_difference );

  if ( abs_clock_difference > MAX_SKEW ) {
    /* avulsion */

    stats_.avulsions++;
    reset( global_ts, local_ts_sample );
    return;
  }

  /* adjust rate to narrow difference */
  if ( abs_clock_difference > 0.1 ) {
    const double derivative = stats_.last_clock_difference - stats_.smoothed_clock_difference;
    const double ideal_clock_rate
      = clock_rate_ + .00000001 * stats_.smoothed_clock_difference + .00001 * derivative;
    clock_rate_ = .1 * CLOCK_SLEW_ALPHA * ideal_clock_rate + ( 1 - .1 * CLOCK_SLEW_ALPHA ) * clock_rate_;

    cout << Timer::timestamp_ns() << " " << stats_.last_clock_difference << " " << stats_.smoothed_clock_difference
         << " " << clock_rate_ << " " << ideal_clock_rate << " " << stats_.smoothed_clock_difference << " "
         << derivative << "\n";
  }
}

double cents( const double val )
{
  return 1200.0 * log2( val );
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
    out << ", offset=";
    pp_samples( out, offset() );
    out << ", rate=" << fixed << setprecision( 2 ) << 1000 * cents( rate() ) << " millicents";

    out << ", diff: cur=" << fixed << setprecision( 1 ) << stats_.last_clock_difference;
    out << ", smoothed=" << fixed << setprecision( 1 ) << stats_.smoothed_clock_difference;
    out << ", biggest=" << fixed << setprecision( 1 ) << stats_.biggest_diff_since_reset;
  }
  out << "\n";
}
