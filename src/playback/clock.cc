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
  , local_clock_( 0 )
  , synced_( false )
{}

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
  local_clock_ += global_ticks_elapsed;

  const double gap_magnitude = local_clock_ - local_ts_last_sample_;

  if ( gap_magnitude > MAX_GAP ) {
    synced_ = false;
  }
}

void Clock::new_sample( const uint64_t global_ts, const uint64_t local_ts_sample )
{
  global_ts_last_update_ = global_ts;
  local_clock_ = local_ts_last_sample_ = local_ts_sample;
  synced_ = true;
}

void Clock::summary( ostream& out ) const
{
  out << "Clock " << ( synced() ? "synced" : "unsynced" ) << "\n";
}
