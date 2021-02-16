#include "encoder_task.hh"

using namespace std;

template class EncoderTask<AudioDeviceTask>;

template<class AudioSource>
EncoderTask<AudioSource>::EncoderTask( const int bit_rate1,
                                       const int bit_rate2,
                                       const int sample_rate,
                                       const shared_ptr<AudioSource> source,
                                       EventLoop& loop )
  : OpusEncoderProcess( bit_rate1, bit_rate2, sample_rate )
  , source_( source )
{
  loop.add_rule(
    "encode [ch1]",
    [&] {
      enc1_.encode_one_frame( source_->capture().ch1() );
      pop_from_source();
    },
    [&] { return enc1_.can_encode_frame( source_->cursor() ); } );

  loop.add_rule(
    "encode [ch2]",
    [&] {
      enc2_.encode_one_frame( source_->capture().ch2() );
      pop_from_source();
    },
    [&] { return enc2_.can_encode_frame( source_->cursor() ); } );
}

template<class AudioSource>
void EncoderTask<AudioSource>::pop_from_source()
{
  const auto encode_cursor = min_encode_cursor();
  if ( encode_cursor > source_->capture().range_begin() ) {
    source_->capture().pop_before( encode_cursor );
  }
}

OpusEncoderProcess::OpusEncoderProcess( const int bit_rate1, const int bit_rate2, const int sample_rate )
  : enc1_( bit_rate1, sample_rate )
  , enc2_( bit_rate2, sample_rate )
{}

OpusEncoderProcess::Channel::Channel( const int bit_rate, const int sample_rate )
  : enc_( bit_rate, sample_rate, 1, OPUS_APPLICATION_RESTRICTED_LOWDELAY )
{}

bool OpusEncoderProcess::Channel::can_encode_frame( const size_t source_cursor ) const
{
  return ( source_cursor >= cursor() + opus_frame::NUM_SAMPLES_MINLATENCY ) and ( not output_.has_value() );
}

void OpusEncoderProcess::Channel::encode_one_frame( const AudioChannel& channel )
{
  if ( output_.has_value() ) {
    throw runtime_error( "internal error: encode_one_frame called but output already has value" );
  }

  output_.emplace();
  enc_.encode( channel.region( cursor(), opus_frame::NUM_SAMPLES_MINLATENCY ), output_.value() );
  num_pushed_++;
}

void OpusEncoderProcess::Channel::reset( const int bit_rate, const int sample_rate )
{
  enc_ = { bit_rate, sample_rate, 1, OPUS_APPLICATION_RESTRICTED_LOWDELAY };
}

void OpusEncoderProcess::reset( const int bit_rate1, const int bit_rate2, const int sample_rate )
{
  enc1_.reset( bit_rate1, sample_rate );
  enc2_.reset( bit_rate2, sample_rate );
}

void OpusEncoderProcess::encode_one_frame( const AudioChannel& ch1, const AudioChannel& ch2 )
{
  enc1_.encode_one_frame( ch1 );
  enc2_.encode_one_frame( ch2 );
}
