#include "cursor.hh"
#include "ewma.hh"

#include <iostream>

using namespace std;
using Option = RubberBand::RubberBandStretcher::Option;

Cursor::Cursor( const uint32_t target_lag_samples, const uint32_t max_lag_samples )
  : target_lag_samples_( target_lag_samples )
  , max_lag_samples_( max_lag_samples )
  , stretcher_( 48000,
                2,
                Option::OptionProcessRealTime | Option::OptionThreadingNever | Option::OptionPitchHighConsistency )
{
  stretcher_.setMaxProcessSize( opus_frame::NUM_SAMPLES_MINLATENCY );
}

void Cursor::miss()
{
  ewma_update( stats_.quality, 0.0, 0.01 );
}

void Cursor::hit()
{
  ewma_update( stats_.quality, 1.0, 0.01 );
}

void Cursor::sample( const PartialFrameStore& frames,
                     const size_t global_sample_index,
                     const size_t frontier_sample_index,
                     const size_t safe_sample_index,
                     OpusDecoderProcess& decoder,
                     ChannelPair& output )
{
  /* initialize cursor if necessary */
  if ( not cursor_location_.has_value() ) {
    cursor_location_ = frontier_sample_index - target_lag_samples_;
    num_samples_output_ = global_sample_index;
    stats_.resets++;
  }

  /* adjust cursor if necessary */
  if ( cursor_location_.value() > int64_t( frontier_sample_index ) ) {
    /* underflow, reset */
    cursor_location_ = frontier_sample_index - target_lag_samples_;
    stats_.resets++;
  }

  ewma_update( stats_.mean_margin_to_frontier, int64_t( frontier_sample_index ) - cursor_location_.value(), 0.01 );
  ewma_update( stats_.mean_margin_to_safe_index, int64_t( safe_sample_index ) - cursor_location_.value(), 0.01 );

  /* do we owe any samples to the output? */
  while ( global_sample_index > num_samples_output_ ) {
    /* we owe some samples to the output -- how many? */
    if ( output.range_end() < num_samples_output_ + opus_frame::NUM_SAMPLES_MINLATENCY ) {
      return;
    }

    /* not enough buffer accumulated yet */
    if ( cursor_location_.value() < 0 ) {
      miss();
      decoder.decode_missing( output.ch1().region( num_samples_output_, opus_frame::NUM_SAMPLES_MINLATENCY ),
                              output.ch2().region( num_samples_output_, opus_frame::NUM_SAMPLES_MINLATENCY ) );
      num_samples_output_ += opus_frame::NUM_SAMPLES_MINLATENCY;
      cursor_location_.value() += opus_frame::NUM_SAMPLES_MINLATENCY;
      continue;
    }

    /* okay, we know where to get them from. Do we have an Opus frame ready to decode? */
    const uint32_t frame_no = cursor_location_.value() / opus_frame::NUM_SAMPLES_MINLATENCY;
    if ( not frames.has_value( cursor_location_.value() / opus_frame::NUM_SAMPLES_MINLATENCY ) ) {
      miss();
      decoder.decode_missing( output.ch1().region( num_samples_output_, opus_frame::NUM_SAMPLES_MINLATENCY ),
                              output.ch2().region( num_samples_output_, opus_frame::NUM_SAMPLES_MINLATENCY ) );
      num_samples_output_ += opus_frame::NUM_SAMPLES_MINLATENCY;
      cursor_location_.value() += opus_frame::NUM_SAMPLES_MINLATENCY;
      continue;
    }

    /* decode a frame! */
    hit();

    array<float, opus_frame::NUM_SAMPLES_MINLATENCY> ch1, ch2;
    span<float> ch1_span { ch1.data(), ch1.size() }, ch2_span { ch2.data(), ch2.size() };

    if ( frames.at( frame_no ).value().separate_channels ) {
      decoder.decode(
        frames.at( frame_no ).value().frame1, frames.at( frame_no ).value().frame2, ch1_span, ch2_span );
    } else {
      decoder.decode_stereo( frames.at( frame_no ).value().frame1, ch1_span, ch2_span );
    }

    /* XXX copy to output */
    output.ch1().region( num_samples_output_, opus_frame::NUM_SAMPLES_MINLATENCY ).copy( ch1_span );
    output.ch2().region( num_samples_output_, opus_frame::NUM_SAMPLES_MINLATENCY ).copy( ch2_span );

    num_samples_output_ += opus_frame::NUM_SAMPLES_MINLATENCY;
    cursor_location_.value() += opus_frame::NUM_SAMPLES_MINLATENCY;
  }
}

void Cursor::summary( ostream& out ) const
{
  out << "Cursor: ";
  out << " target lag=" << target_lag_samples_;
  out << " actual lag=" << stats_.mean_margin_to_frontier;
  out << " safety margin=" << stats_.mean_margin_to_safe_index;
  out << " quality=" << fixed << setprecision( 5 ) << stats_.quality;
  out << " resets=" << stats_.resets;
  out << "\n";
}

size_t Cursor::ok_to_pop( const PartialFrameStore& frames ) const
{
  if ( not cursor_location_.has_value() ) {
    return 0;
  }

  const size_t next_frame_needed = cursor_location_.value() / opus_frame::NUM_SAMPLES_MINLATENCY;
  if ( next_frame_needed <= frames.range_begin() ) {
    return 0;
  }

  return next_frame_needed - frames.range_begin();
}
