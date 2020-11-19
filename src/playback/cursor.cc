#include "cursor.hh"
#include "timestamp.hh"

using namespace std;

void Cursor::sample( const NetworkEndpoint& endpoint, const uint64_t now, const size_t sample_index )
{
  /* initialize if necessary */
  if ( not initialized ) {
    if ( endpoint.unreceived_beyond_this_frame_index() > next_frame_index_ ) {
      next_frame_index_ts_ = now + initial_delay_ms_ * MILLION;
      next_frame_index_ = endpoint.unreceived_beyond_this_frame_index();
      average_safety_margin_ = initial_delay_ms_;
      average_safety_margin_slow_ = initial_delay_ms_;
      sample_index_ = sample_index;
      initialized = true;
    }
    return;
  }

  /* decode frame, and calculate quality score and safety margin */
  while ( now >= next_frame_index_ts_ ) {
    const bool success = has_frame( endpoint, next_frame_index_ );

    if ( success ) {
      const AudioFrame& frame = endpoint.frames().at( next_frame_index_ ).value();
      output_.decode( frame.ch1, frame.ch2, sample_index_ );
    }

    quality_ = ALPHA * success + ( 1 - ALPHA ) * quality_;

    const float this_safety_margin
      = 2.5 * ( float( endpoint.unreceived_beyond_this_frame_index() ) - float( next_frame_index_ ) );
    average_safety_margin_ = ALPHA * this_safety_margin + ( 1 - ALPHA ) * average_safety_margin_;
    average_safety_margin_slow_ = ALPHA * this_safety_margin + ( 1 - ALPHA ) * average_safety_margin_slow_;

    // do we need to make a big jump?
    if ( abs( initial_delay_ms_ - this_safety_margin ) > 0.75 * initial_delay_ms_ ) {
      initialized = false;
      next_frame_index_ = endpoint.unreceived_beyond_this_frame_index();
      if ( adjust_delay_ ) {
        if ( initial_delay_ms_ > this_safety_margin ) {
          initial_delay_ms_ += 5;
        }
      }
      resets++;
      return;
    }

    if ( adjust_delay_ and ( now >= next_adjustment_ts_ ) ) {
      if ( quality_ < 0.98 ) {
        initial_delay_ms_ += 0.1;
        next_adjustment_ts_ = now + 10 * MILLION;
      } else if ( ( quality_ > 0.998 ) and ( initial_delay_ms_ > 10 ) and ( average_safety_margin_slow_ > 10 ) ) {
        initial_delay_ms_ -= 0.005;
        next_adjustment_ts_ = now + 10 * MILLION;
      }
    }

    // okay, maybe a small jump?
    uint64_t increment = 2500000;
    if ( ( initial_delay_ms_ - average_safety_margin_ ) > 0.25 * initial_delay_ms_ ) {
      increment += 20833; /* about one sample */
      sample_index_++;
      inc_plus++;
    } else if ( ( average_safety_margin_ - initial_delay_ms_ ) > 0.25 * initial_delay_ms_ ) {
      increment -= 20833;
      sample_index_--;
      inc_minus++;
    }

    // advance cursor
    next_frame_index_ts_ += increment;
    next_frame_index_++;
    sample_index_ += 120;
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
      << setprecision( 0 ) << fixed << average_safety_margin_ << "+" << inc_plus << "-" << inc_minus << "#"
      << resets << " ";
  pp_samples( out, sample_index_ );
}
