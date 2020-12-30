#include "cursor.hh"

using namespace std;

void Cursor::sample( const PartialFrameStore& frames,
                     const size_t global_sample_index,
                     const std::optional<size_t> local_clock_sample_index,
                     const float jitter_samples,
                     AudioBuffer& output )
{
  /* initialize cursor if necessary */
  if ( local_clock_sample_index.has_value() ) {
    if ( ( not cursor_location_.has_value() ) and ( local_clock_sample_index.value() >= target_lag_samples_ ) ) {
      cursor_location_ = local_clock_sample_index.value() - target_lag_samples_;
      slew_ = Slew::NO;
      stats_.resets++;
    }
  } else {
    cursor_location_.reset();
  }

  /* adjust lag if necessary */
  if ( minimize_lag_ ) {
    if ( quality_ > 0.9999 and target_lag_samples_ > max( jitter_samples * 5, 240.0f ) ) {
      target_lag_samples_--;
    }

    if ( target_lag_samples_ < jitter_samples * 2 ) {
      target_lag_samples_++;
    }
  }

  /* adjust cursor if necessary */
  if ( cursor_location_.has_value() ) {
    const int64_t cursor_skew = local_clock_sample_index.value() - target_lag_samples_ - cursor_location_.value();
    if ( cursor_skew > 300 or cursor_skew < -300 ) {
      /* reset cursor */
      cursor_location_ = local_clock_sample_index.value() - target_lag_samples_;
      slew_ = Slew::NO;
      stats_.resets++;
    } else if ( cursor_skew > 180 ) {
      slew_ = Slew::CONSUME_FASTER;
    } else if ( cursor_skew < -180 ) {
      slew_ = Slew::CONSUME_SLOWER;
    } else if ( cursor_skew <= 48 and cursor_skew >= -48 ) {
      slew_ = Slew::NO;
    }
    stats_.last_skew = cursor_skew;
  }

  /* do we owe any samples to the output? */
  while ( global_sample_index > num_samples_output_ ) {
    /* we owe some samples to the output -- how many? */

    /* do we even know where to get them from? */
    if ( not cursor_location_.has_value() ) {
      miss();
      decoder_.decode_missing( num_samples_output_, output );
      num_samples_output_ += opus_frame::NUM_SAMPLES;
      continue;
    }

    uint8_t samples_to_output = opus_frame::NUM_SAMPLES;
    switch ( slew_ ) {
      case Slew::NO:
        break;
      case Slew::CONSUME_FASTER:
        samples_to_output--;
        stats_.samples_skipped++;
        break;
      case Slew::CONSUME_SLOWER:
        samples_to_output++;
        stats_.samples_inserted++;
        break;
    }

    /* okay, we know where to get them from. Do we have an Opus frame ready to decode? */
    const uint32_t frame_no = cursor_location_.value() / opus_frame::NUM_SAMPLES;
    if ( not frames.has_value( cursor_location_.value() / opus_frame::NUM_SAMPLES ) ) {
      miss();
      decoder_.decode_missing( num_samples_output_, output );
      num_samples_output_ += samples_to_output;
      cursor_location_.value() += opus_frame::NUM_SAMPLES;
      continue;
    }

    /* decode a frame! */
    hit();
    decoder_.decode(
      frames.at( frame_no ).value().ch1, frames.at( frame_no ).value().ch2, num_samples_output_, output );
    num_samples_output_ += samples_to_output;
    cursor_location_.value() += opus_frame::NUM_SAMPLES;
  }
}

void Cursor::summary( ostream& out ) const
{
  out << "Cursor: ";
  out << " target lag=" << target_lag_samples_;
  out << " quality=" << fixed << setprecision( 5 ) << quality_;
  out << " inserted=" << stats_.samples_inserted;
  out << " skipped=" << stats_.samples_skipped;
  out << " resets=" << stats_.resets;
  out << " slew=" << int( slew_ );
  out << " last_skew=" << int( stats_.last_skew );
  out << " ignored/success/missing=" << decoder_.stats().ignored_decodes << "/"
      << decoder_.stats().successful_decodes << "/" << decoder_.stats().missing_decodes;
  out << "\n";
}

size_t Cursor::ok_to_pop( const PartialFrameStore& frames ) const
{
  if ( not cursor_location_.has_value() ) {
    return 0;
  }

  const size_t next_frame_needed = cursor_location_.value() / opus_frame::NUM_SAMPLES;
  if ( next_frame_needed <= frames.range_begin() ) {
    return 0;
  }

  return next_frame_needed - frames.range_begin();
}
