#include "cursor.hh"
#include "ewma.hh"

#include <iostream>

using namespace std;

Cursor::Cursor( const uint32_t target_lag_samples, const uint32_t min_lag_samples, const uint32_t max_lag_samples )
  : target_lag_samples_( target_lag_samples )
  , min_lag_samples_( min_lag_samples )
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

void Cursor::setup( const size_t global_sample_index, const size_t frontier_sample_index )
{
  /* initialize cursor if necessary */
  if ( not frame_cursor_.has_value() and frontier_sample_index > target_lag_samples_ ) {
    frame_cursor_ = ( frontier_sample_index - target_lag_samples_ ) / opus_frame::NUM_SAMPLES_MINLATENCY;
    num_samples_output_ = global_sample_index;
    rate_ = Rate::Steady;
    stats_.resets++;
  }
}

void Cursor::sample( const PartialFrameStore<AudioFrame>& frames,
                     const size_t frontier_sample_index,
                     OpusDecoderProcess& decoder,
                     RubberBand::RubberBandStretcher& stretcher,
                     AudioSlice& output )
{
  if ( not initialized() ) {
    throw runtime_error( "Cursor::sample() called on uninitialized Cursor" );
  }

  /* adjust cursor if necessary */
  if ( greatest_read_location() >= frontier_sample_index ) {
    /* underflow, reset */
    if ( frontier_sample_index < target_lag_samples_ ) {
      /* not enough audio? */
      frame_cursor_.reset();
      num_samples_output_.reset();
      output.good = false;
      return;
    }

    frame_cursor_ = ( frontier_sample_index - target_lag_samples_ ) / opus_frame::NUM_SAMPLES_MINLATENCY;
    rate_ = Rate::Steady;
    if ( greatest_read_location() >= frontier_sample_index ) {
      throw runtime_error( "internal error" );
    }
    stats_.resets++;
  }

  /* sample statistics */
  const uint64_t frame_cursor = frame_cursor_.value();

  const int64_t margin_to_frontier = frontier_sample_index - greatest_read_location();
  ewma_update( stats_.mean_margin_to_frontier, margin_to_frontier, ALPHA );

  /* adjust stretching behavior */

  /* 1) should we stop compressing? */
  if ( rate_ == Rate::Compressing and ( margin_to_frontier <= target_lag_samples_ ) ) {
    rate_ = Rate::Steady;
    stretcher.setTimeRatio( 1.00 );
    stats_.compress_stops++;
  }

  /* 2) should we stop expanding? */
  if ( rate_ == Rate::Expanding and ( margin_to_frontier >= target_lag_samples_ ) ) {
    rate_ = Rate::Steady;
    stretcher.setTimeRatio( 1.00 );
    stats_.expand_stops++;
  }

  /* 3) should we start compressing or expanding? */
  if ( rate_ == Rate::Steady ) {
    if ( ( margin_to_frontier > max_lag_samples_ ) and ( stats_.mean_margin_to_frontier > max_lag_samples_ ) ) {
      rate_ = Rate::Compressing;
      stretcher.setTimeRatio( 0.95 );
      stats_.compress_starts++;
    } else if ( ( margin_to_frontier < min_lag_samples_ )
                and ( stats_.mean_margin_to_frontier < min_lag_samples_ ) ) {
      rate_ = Rate::Expanding;
      stretcher.setTimeRatio( 1.05 );
      stats_.expand_starts++;
    }
  }

  ewma_update( stats_.mean_time_ratio, stretcher.getTimeRatio(), ALPHA );

  array<float, opus_frame::NUM_SAMPLES_MINLATENCY> ch1_scratch, ch2_scratch;
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
  array<float*, 2> decoded_audio_for_stretcher = { ch1_scratch.data(), ch2_scratch.data() };
  stretcher.process( decoded_audio_for_stretcher.data(), opus_frame::NUM_SAMPLES_MINLATENCY, false );

  const int samples_available = stretcher.available();
  if ( samples_available < 0 ) {
    throw runtime_error( "stretcher.available() < 0" );
  }
  const size_t samples_out = samples_available;

  if ( samples_out > output.ch1.size() ) {
    throw runtime_error( "stretcher output exceeds available output size" );
  }

  array<float*, 2> stretched_audio_from_stretcher = { output.ch1.data(), output.ch2.data() };
  if ( samples_out != stretcher.retrieve( stretched_audio_from_stretcher.data(), samples_out ) ) {
    throw runtime_error( "unexpected output from stretcher.retrieve()" );
  }

  output.sample_index = num_samples_output_.value();
  output.length = samples_out;
  output.good = true;

  num_samples_output_.value() += samples_out;
  ++frame_cursor_.value();
}

void Cursor::summary( ostream& out ) const
{
  out << "Cursor: ";
  out << " target lag=" << target_lag_samples_;
  out << " actual lag=" << stats_.mean_margin_to_frontier;
  out << " quality=" << fixed << setprecision( 5 ) << stats_.quality;
  out << " time ratio=" << fixed << setprecision( 5 ) << stats_.mean_time_ratio;
  out << " compressions=" << stats_.compress_starts << "+" << stats_.compress_stops;
  out << " expansions=" << stats_.expand_starts << "+" << stats_.expand_stops;
  out << " rate=" << int( rate_ );
  out << " resets=" << stats_.resets;
  out << "\n";
}

void Cursor::json_summary( Json::Value& root ) const
{
  root["target_lag"] = target_lag_samples_;
  root["actual_lag"] = stats_.mean_margin_to_frontier;
  root["quality"] = stats_.quality;
  root["min_lag"] = min_lag_samples_;
  root["max_lag"] = max_lag_samples_;
  root["resets"] = stats_.resets;
  root["compressions"] = stats_.compress_starts;
  root["expansions"] = stats_.expand_starts;
}

void Cursor::default_json_summary( Json::Value& root )
{
  root["target_lag"] = 0;
  root["actual_lag"] = 0;
  root["quality"] = 0;
  root["min_lag"] = 0;
  root["max_lag"] = 0;
  root["resets"] = 0;
  root["compressions"] = 0;
  root["expansions"] = 0;
}

size_t Cursor::ok_to_pop( const PartialFrameStore<AudioFrame>& frames ) const
{
  if ( not frame_cursor_.has_value() ) {
    return 0;
  }

  if ( frame_cursor_.value() <= frames.range_begin() ) {
    return 0;
  }

  return frame_cursor_.value() - frames.range_begin();
}
