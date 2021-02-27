#include "audioboard.hh"
#include "ewma.hh"

using namespace std;

AudioBoard::AudioBoard( const string_view name, const uint8_t num_channels )
  : name_( name )
{
  channels_.reserve( num_channels );
  gains_.reserve( num_channels );
  for ( uint8_t i = 0; i < num_channels; i++ ) {
    channels_.emplace_back( "Unknown " + to_string( i ), AudioChannel { 8192 } );
    gains_.push_back( { 2.0, 2.0 } );
    power_.push_back( 0.0 );
  }
}

void AudioBoard::set_gain( const string_view channel_name, const float gain1, const float gain2 )
{
  for ( uint8_t channel_i = 0; channel_i < num_channels(); channel_i++ ) {
    if ( channels_.at( channel_i ).first == channel_name ) {
      gains_.at( channel_i ) = { gain1, gain2 };
    }
  }
}

void AudioBoard::pop_samples_until( const uint64_t sample )
{
  for ( uint8_t channel_i = 0; channel_i < num_channels(); channel_i++ ) {
    AudioChannel& channel = channels_.at( channel_i ).second;

    for ( uint64_t index = channel.range_begin(); index < sample; index++ ) {
      const float mixed_sample_val = channel.at( index ) * ( gain( channel_i ).first + gain( channel_i ).second );
      ewma_update( power_.at( channel_i ), mixed_sample_val * mixed_sample_val, 0.0002 );
    }

    channel.pop_before( sample );
  }
}

void AudioBoard::json_summary( Json::Value& root ) const
{
  root["name"] = name_;
  for ( unsigned int i = 0; i < num_channels(); i++ ) {
    root["channels"][channels_.at( i ).first]["amplitude"] = float_to_dbfs( sqrt( power_.at( i ) ) );
    const float gain_sum = gains_.at( i ).first + gains_.at( i ).second;
    root["channels"][channels_.at( i ).first]["gain"] = float_to_dbfs( gain_sum );
    root["channels"][channels_.at( i ).first]["pan"] = 2 * ( ( gains_.at( i ).second / gain_sum ) - 0.5 );
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

      const auto [gain_into_1, gain_into_2] = board.gain( channel_i );
      for ( uint8_t sample_i = 0; sample_i < opus_frame::NUM_SAMPLES_MINLATENCY; sample_i++ ) {
        const float value = other_channel[sample_i];
        const float orig_1 = ch1_target[sample_i];
        const float orig_2 = ch2_target[sample_i];

        ch1_target[sample_i] = orig_1 + gain_into_1 * value;
        ch2_target[sample_i] = orig_2 + gain_into_2 * value;
      }
    }

    encoder_.encode_one_frame( mixed_audio_.ch1(), mixed_audio_.ch2() );
    auto frame_mutable = encoder_.front( 0 );
    socket_.sendto_ignore_errors( destination_, frame_mutable.frame1 );
    encoder_.pop_frame();
    mix_cursor_ += opus_frame::NUM_SAMPLES_MINLATENCY;
    mixed_audio_.pop_before( mix_cursor_ );
  }
}

AudioWriter::AudioWriter( const string_view socket_path )
  : destination_( Address::abstract_unix( socket_path ) )
{
  socket_.set_blocking( false );
}
