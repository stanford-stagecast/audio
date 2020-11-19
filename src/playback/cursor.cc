#include "cursor.hh"

using namespace std;

void Cursor::sample( const NetworkEndpoint& endpoint, const uint64_t now )
{
  /* initialize if necessary */
  if ( not initialized ) {
    if ( endpoint.unreceived_beyond_this_frame_index() > 0 ) {
      next_frame_index_ts_ = now + initial_delay_ms_ * MILLION;
      next_frame_index_ = endpoint.unreceived_beyond_this_frame_index();
      average_safety_margin_ = initial_delay_ms_;
      average_safety_margin_slow_ = initial_delay_ms_;
      initialized = true;
    }
    return;
  }

  /* calculate quality score and safety margin */
  while ( now >= next_frame_index_ts_ ) {
    const bool success = has_frame( endpoint, next_frame_index_ );
    quality_ = ALPHA * success + ( 1 - ALPHA ) * quality_;

    const float this_safety_margin
      = 2.5 * ( float( endpoint.unreceived_beyond_this_frame_index() ) - float( next_frame_index_ ) );
    average_safety_margin_ = ALPHA * this_safety_margin + ( 1 - ALPHA ) * average_safety_margin_;
    average_safety_margin_slow_ = ALPHA * this_safety_margin + ( 1 - ALPHA ) * average_safety_margin_slow_;

    // do we need to make a big jump?
    if ( abs( initial_delay_ms_ - this_safety_margin ) > 0.75 * initial_delay_ms_ ) {
      initialized = false;
      resets++;
      return;
    }

    uint64_t increment = 2500000;
    if ( adjust_delay_ ) {
      if ( ( initial_delay_ms_ - average_safety_margin_slow_ ) > 0.5 * initial_delay_ms_ ) {
        initial_delay_ms_ *= 1.05;
      } else if ( ( average_safety_margin_slow_ - initial_delay_ms_ ) > 0.5 * initial_delay_ms_ ) {
        initial_delay_ms_ /= 1.05;
      }
    }

    // okay, maybe a small jump?
    if ( ( initial_delay_ms_ - average_safety_margin_ ) > 0.25 * initial_delay_ms_ ) {
      increment += 20833; /* about one sample */
      inc_plus++;
    } else if ( ( average_safety_margin_ - initial_delay_ms_ ) > 0.25 * initial_delay_ms_ ) {
      increment -= 20833;
      inc_minus++;
    }

    // advance cursor
    next_frame_index_ts_ += increment;
    next_frame_index_++;
  }
}

bool Cursor::has_frame( const NetworkEndpoint& endpoint, const uint32_t frame_index )
{
  if ( frame_index < endpoint.frames().range_begin() ) {
    return false;
  }

  if ( frame_index >= endpoint.unreceived_beyond_this_frame_index() ) {
    return false;
  }

  return endpoint.frames().at( frame_index ).has_value();
}

void Cursor::summary( ostream& out ) const
{
  out << " " << setprecision( 0 ) << fixed << 100 * quality() << "@" << initial_delay_ms() << "|"
      << setprecision( 0 ) << fixed << safety_margin_ms() << "+" << inc_plus << "-" << inc_minus << "#" << resets;
}
