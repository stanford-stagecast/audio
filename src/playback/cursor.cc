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

  /* do we owe any samples to the output? */
  while ( global_sample_index > num_samples_output_ ) {
    /* adjust cursor if necessary */
    if ( cursor_location_.value() > int64_t( frontier_sample_index ) ) {
      /* underflow, reset */
      cursor_location_ = frontier_sample_index - target_lag_samples_;
      stats_.resets++;
    }

    ewma_update(
      stats_.mean_margin_to_frontier, int64_t( frontier_sample_index ) - cursor_location_.value(), 0.01 );
    ewma_update( stats_.mean_margin_to_safe_index, int64_t( safe_sample_index ) - cursor_location_.value(), 0.01 );

    /* we owe some samples to the output -- how many? */
    if ( output.range_end() < num_samples_output_ + opus_frame::NUM_SAMPLES_MINLATENCY ) {
      throw runtime_error( "samples owed exceeds available storage" );
    }

    /* not enough buffer accumulated yet */
    if ( cursor_location_.value() < 0 ) {
      miss();
      num_samples_output_ += opus_frame::NUM_SAMPLES_MINLATENCY;
      cursor_location_.value() += opus_frame::NUM_SAMPLES_MINLATENCY;
      continue;
    }

    array<float, 1024> ch1_scratch, ch2_scratch;
    array<float*, 2> scratch = { ch1_scratch.data(), ch2_scratch.data() };
    span<float> ch1_decoded { ch1_scratch.data(), opus_frame::NUM_SAMPLES_MINLATENCY };
    span<float> ch2_decoded { ch2_scratch.data(), opus_frame::NUM_SAMPLES_MINLATENCY };

    /* okay, we know where to get them from. Do we have an Opus frame ready to decode? */
    const uint32_t frame_no = cursor_location_.value() / opus_frame::NUM_SAMPLES_MINLATENCY;
    if ( not frames.has_value( cursor_location_.value() / opus_frame::NUM_SAMPLES_MINLATENCY ) ) {
      /* leave it as silence */
      fill( ch1_decoded.begin(), ch1_decoded.end(), 0 );
      fill( ch2_decoded.begin(), ch2_decoded.end(), 0 );
      miss();
    } else {
      /* decode a frame! */
      hit();

      if ( frames.at( frame_no ).value().separate_channels ) {
        decoder.decode(
          frames.at( frame_no ).value().frame1, frames.at( frame_no ).value().frame2, ch1_decoded, ch2_decoded );
      } else {
        decoder.decode_stereo( frames.at( frame_no ).value().frame1, ch1_decoded, ch2_decoded );
      }
    }

    /* time-stretch */
    stretcher_.process( scratch.data(), opus_frame::NUM_SAMPLES_MINLATENCY, false );

    const int samples_available = stretcher_.available();
    if ( samples_available < 0 ) {
      throw runtime_error( "stretcher_.available() < 0" );
    }
    const size_t samples_out = samples_available;

    if ( samples_out > ch1_scratch.size() ) {
      throw runtime_error( "stretcher has too many samples available (" + to_string( samples_out ) + ")" );
    }

    if ( num_samples_output_ + samples_out > output.ch1().range_end() ) {
      throw runtime_error( "stretcher output exceeds available output size" );
    }

    if ( samples_out != stretcher_.retrieve( scratch.data(), samples_out ) ) {
      throw runtime_error( "unexpected output from stretcher_.retrieve()" );
    }

    const span_view<float> ch1_stretched { ch1_scratch.data(), samples_out };
    const span_view<float> ch2_stretched { ch2_scratch.data(), samples_out };

    /* copy to output */
    output.ch1().region( num_samples_output_, samples_out ).copy( ch1_stretched );
    output.ch2().region( num_samples_output_, samples_out ).copy( ch2_stretched );

    num_samples_output_ += samples_out;
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
