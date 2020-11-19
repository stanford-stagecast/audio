#include "cursor.hh"

using namespace std;

void Cursor::sample( const NetworkEndpoint& endpoint, const uint64_t now )
{
  /* initialize if necessary */
  if ( not initialized ) {
    if ( endpoint.unreceived_beyond_this_frame_index() > 0 ) {
      next_frame_index_ts_ = now + initial_delay_ns_;
      average_safety_margin = initial_delay_ns_;
      initialized = true;
    }
    return;
  }

  /* calculate quality score and safety margin */
  if ( now >= next_frame_index_ts_ ) {
    const bool success = has_frame( endpoint, next_frame_index_ );
    quality = ALPHA * success + ( 1 - ALPHA ) * quality;

    const float this_safety_margin = ( endpoint.next_frame_needed() - next_frame_index_ ) * 2.5 * MILLION;
    average_safety_margin = ALPHA * this_safety_margin + ( 1 - ALPHA ) * average_safety_margin;
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
