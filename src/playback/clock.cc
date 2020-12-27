#include "clock.hh"
#include "exception.hh"
#include "timestamp.hh"

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
  resets_++;
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

  if ( abs( local_clock_ - local_ts_last_sample_ ) > MAX_DIFFERENCE ) {
    synced_ = false;
    gaps_++;
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

  last_clock_difference_ = local_ts_sample - local_clock_;

  if ( abs( last_clock_difference_ ) > MAX_DIFFERENCE ) {
    /* avulsion */

    avulsions_++;
    reset( global_ts, local_ts_sample );
    return;
  }

  /* adjust rate to narrow difference */
  const double ideal_clock_rate = clock_rate_ + last_clock_difference_ / CLOCK_SLEW_HORIZON;

  clock_rate_ = CLOCK_SLEW_ALPHA * ideal_clock_rate + ( 1 - CLOCK_SLEW_ALPHA ) * clock_rate_;
}

void Clock::summary( ostream& out ) const
{
  out << "Clock " << ( synced() ? "synced" : "unsynced" );
  out << " resets=" << resets_ << " (" << gaps_ << " gaps, " << avulsions_ << " avulsions)";
  if ( synced() ) {
    out << " offset=";
    pp_samples( out, offset() );
    out << ", rate=" << fixed << setprecision( 10 ) << rate();
    out << ", diff=" << last_clock_difference_ << "\n";
  }
  out << "\n";
}
