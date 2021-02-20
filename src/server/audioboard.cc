#include "audioboard.hh"

using namespace std;

AudioBoard::AudioBoard( const uint8_t num_channels )
{
  channels_.reserve( num_channels );
  for ( uint8_t i = 0; i < num_channels; i++ ) {
    channels_.emplace_back( "Unknown " + to_string( i ), AudioChannel { 8192 } );
  }
}

void AudioBoard::pop_samples_until( const uint64_t sample )
{
  for ( auto& buf : channels_ ) {
    buf.second.pop_before( sample );
  }
}

void AudioWriter::mix_and_write( const AudioBoard& board, const uint64_t cursor_sample )
{
  while ( mix_cursor_ + opus_frame::NUM_SAMPLES_MINLATENCY <= cursor_sample ) {
    span<float> ch1_target = mixed_audio_.ch1().region( mix_cursor_, opus_frame::NUM_SAMPLES_MINLATENCY );
    span<float> ch2_target = mixed_audio_.ch2().region( mix_cursor_, opus_frame::NUM_SAMPLES_MINLATENCY );

    for ( uint8_t channel_i = 0; channel_i < board.num_channels(); channel_i++ ) {
      const span_view<float> other_channel
        = board.channel( channel_i ).region( mix_cursor_, opus_frame::NUM_SAMPLES_MINLATENCY );

      const float gain_into_1 = 2.0;
      const float gain_into_2 = 2.0;
      for ( uint8_t sample_i = 0; sample_i < opus_frame::NUM_SAMPLES_MINLATENCY; sample_i++ ) {
        const float value = other_channel[sample_i];
        const float orig_1 = ch1_target[sample_i];
        const float orig_2 = ch2_target[sample_i];

        ch1_target[sample_i] = orig_1 + gain_into_1 * value;
        ch2_target[sample_i] = orig_2 + gain_into_2 * value;
      }
    }

    encoder_.encode_one_frame( mixed_audio_.ch1(), mixed_audio_.ch2() );
    auto frame_mutable = encoder_.front_as_audioframe( 0 );
    webm_writer_.write( frame_mutable.frame1, mix_cursor_ );
    encoder_.pop_frame();
    mix_cursor_ += opus_frame::NUM_SAMPLES_MINLATENCY;
    mixed_audio_.pop_before( mix_cursor_ );
  }
}
