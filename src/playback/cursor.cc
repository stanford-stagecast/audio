#include "cursor.hh"

using namespace std;

void Cursor::sample( const NetworkEndpoint& endpoint, const uint64_t now )
{
  /* initialize if necessary */
  if ( not initialized ) {
    if ( endpoint.unreceived_beyond_this_frame_index() > 0 ) {
      next_frame_index_ts_ = now + initial_delay_ns_;
      average_safety_margin_ = initial_delay_ns_ / float( MILLION );
      initialized = true;
    }
    return;
  }

  /* calculate quality score and safety margin */
  while ( now >= next_frame_index_ts_ ) {
    const bool success = has_frame( endpoint, next_frame_index_ );
    quality_ = ALPHA * success + ( 1 - ALPHA ) * quality_;

    const float this_safety_margin = 2.5 * ( float( endpoint.next_frame_needed() ) - float( next_frame_index_ ) );
    average_safety_margin_ = ALPHA * this_safety_margin + ( 1 - ALPHA ) * average_safety_margin_;

    // advance cursor
    next_frame_index_ts_ += 2.5 * MILLION;
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
