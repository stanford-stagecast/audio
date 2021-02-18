#include "cursor.hh"
#include "ewma.hh"

#include <iostream>

using namespace std;

Cursor::Cursor( const uint32_t target_lag_samples, const uint32_t max_lag_samples )
  : target_lag_samples_( target_lag_samples )
  , max_lag_samples_( max_lag_samples )
{}

void Cursor::miss()
{
  ewma_update( stats_.quality, 0.0, ALPHA );
}

void Cursor::hit()
{
  ewma_update( stats_.quality, 1.0, ALPHA );
}

void Cursor::sample( const PartialFrameStore& frames,
                     const size_t global_sample_index,
                     const size_t frontier_sample_index,
                     const size_t safe_sample_index,
                     OpusDecoderProcess& decoder,
                     RubberBand::RubberBandStretcher& stretcher,
                     ChannelPair& output )
{
  /* initialize cursor if necessary */
  if ( not frame_cursor_.has_value() and frontier_sample_index > target_lag_samples_ ) {
    frame_cursor_ = ( frontier_sample_index - target_lag_samples_ ) / opus_frame::NUM_SAMPLES_MINLATENCY;
    num_samples_output_ = global_sample_index;
    stats_.resets++;
  }

  if ( not frame_cursor_.has_value() ) {
    num_samples_output_ = global_sample_index;
    return;
  }

  /* do we owe any samples to the output? */
  while ( global_sample_index > num_samples_output_ ) {
    const uint64_t frame_cursor = frame_cursor_.value();

    const int64_t margin_to_frontier = frontier_sample_index - greatest_read_location();
    stats_.min_margin_to_frontier = min( stats_.min_margin_to_frontier, margin_to_frontier );
    ewma_update( stats_.mean_margin_to_frontier, margin_to_frontier, ALPHA );
    ewma_update(
      stats_.mean_margin_to_safe_index, int64_t( safe_sample_index ) - int64_t( greatest_read_location() ), ALPHA );

    /* adjust cursor if necessary */
    if ( greatest_read_location() >= frontier_sample_index ) {
      /* underflow, reset */
      if ( frontier_sample_index > target_lag_samples_ ) {
        frame_cursor_ = ( frontier_sample_index - target_lag_samples_ ) / opus_frame::NUM_SAMPLES_MINLATENCY;
        if ( greatest_read_location() >= frontier_sample_index ) {
          throw runtime_error( "internal error" );
        }
        stats_.min_margin_to_frontier = frontier_sample_index - greatest_read_location();
        stats_.resets++;
        continue;
      } else {
        frame_cursor_.reset();
        num_samples_output_ = global_sample_index;
        stats_.resets++;
        return;
      }
    }

    /* should we start speeding up? */
    if ( ( not compressing_ ) and ( stats_.mean_margin_to_frontier > max_lag_samples_ )
         and ( margin_to_frontier > max_lag_samples_ ) ) {
      compressing_ = true;
      stretcher.setTimeRatio( 0.9 );
      stats_.compress_starts++;
    }

    /* should we stop speeding up? */
    if ( compressing_ and ( margin_to_frontier <= target_lag_samples_ ) ) {
      compressing_ = false;
      stretcher.setTimeRatio( 1.00 );
      stats_.compress_stops++;
    }

    ewma_update( stats_.mean_time_ratio, stretcher.getTimeRatio(), ALPHA );

    if ( output.range_end() < num_samples_output_ + opus_frame::NUM_SAMPLES_MINLATENCY ) {
      throw runtime_error( "samples owed exceeds available storage" );
    }

    array<float, 1024> ch1_scratch, ch2_scratch;
    span<float> ch1_decoded { ch1_scratch.data(), opus_frame::NUM_SAMPLES_MINLATENCY };
    span<float> ch2_decoded { ch2_scratch.data(), opus_frame::NUM_SAMPLES_MINLATENCY };

    /* Do we have an Opus frame ready to decode? */
    if ( not frames.has_value( frame_cursor ) ) {
      /* no, so leave it as silence */
      miss();
      fill( ch1_decoded.begin(), ch1_decoded.end(), 0 );
      fill( ch2_decoded.begin(), ch2_decoded.end(), 0 );
    } else {
      /* decode a frame! */
      hit();

      if ( frames.at( frame_cursor ).value().separate_channels ) {
        decoder.decode( frames.at( frame_cursor ).value().frame1,
                        frames.at( frame_cursor ).value().frame2,
                        ch1_decoded,
                        ch2_decoded );
      } else {
        decoder.decode_stereo( frames.at( frame_cursor ).value().frame1, ch1_decoded, ch2_decoded );
      }
    }

    /* time-stretch */
    array<float*, 2> scratch = { ch1_scratch.data(), ch2_scratch.data() };
    stretcher.process( scratch.data(), opus_frame::NUM_SAMPLES_MINLATENCY, false );

    const int samples_available = stretcher.available();
    if ( samples_available < 0 ) {
      throw runtime_error( "stretcher.available() < 0" );
    }
    const size_t samples_out = samples_available;

    if ( samples_out > ch1_scratch.size() ) {
      throw runtime_error( "stretcher has too many samples available (" + to_string( samples_out ) + ")" );
    }

    if ( num_samples_output_ + samples_out > output.ch1().range_end() ) {
      throw runtime_error( "stretcher output exceeds available output size" );
    }

    if ( samples_out != stretcher.retrieve( scratch.data(), samples_out ) ) {
      throw runtime_error( "unexpected output from stretcher.retrieve()" );
    }

    const span_view<float> ch1_stretched { ch1_scratch.data(), samples_out };
    const span_view<float> ch2_stretched { ch2_scratch.data(), samples_out };

    /* copy to output */
    output.ch1().region( num_samples_output_, samples_out ).copy( ch1_stretched );
    output.ch2().region( num_samples_output_, samples_out ).copy( ch2_stretched );

    num_samples_output_ += samples_out;
    ++frame_cursor_.value();
  }
}

void Cursor::summary( ostream& out ) const
{
  out << "Cursor: ";
  out << " target lag=" << target_lag_samples_;
  out << " actual lag=" << stats_.mean_margin_to_frontier;
  out << " min lag=" << stats_.min_margin_to_frontier;
  out << " safety margin=" << stats_.mean_margin_to_safe_index;
  out << " quality=" << fixed << setprecision( 5 ) << stats_.quality;
  out << " time ratio=" << fixed << setprecision( 5 ) << stats_.mean_time_ratio;
  out << " compression starts=" << stats_.compress_starts << " stops=" << stats_.compress_stops;
  out << " resets=" << stats_.resets;
  out << "\n";
}

size_t Cursor::ok_to_pop( const PartialFrameStore& frames ) const
{
  if ( not frame_cursor_.has_value() ) {
    return 0;
  }

  if ( frame_cursor_.value() <= frames.range_begin() ) {
    return 0;
  }

  return frame_cursor_.value() - frames.range_begin();
}
