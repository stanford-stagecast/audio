#include "encoder_task.hh"

using namespace std;

template class EncoderTask<AudioDeviceTask>;

template<class AudioSource>
EncoderTask<AudioSource>::EncoderTask( const int bit_rate,
                                       const int sample_rate,
                                       const shared_ptr<AudioSource> source,
                                       EventLoop& loop )
  : OpusEncoderProcess( bit_rate, sample_rate )
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
    source_->capture().pop( encode_cursor - source_->capture().range_begin() );
  }
}

OpusEncoderProcess::OpusEncoderProcess( const int bit_rate, const int sample_rate )
  : enc1_( bit_rate, sample_rate )
  , enc2_( bit_rate, sample_rate )
{}

OpusEncoderProcess::Channel::Channel( const int bit_rate, const int sample_rate )
  : enc_( bit_rate, sample_rate )
{}

bool OpusEncoderProcess::Channel::can_encode_frame( const size_t source_cursor ) const
{
  return ( source_cursor > cursor() + opus_frame::NUM_SAMPLES ) and ( output_.writable_region().size() > 0 );
}

void OpusEncoderProcess::Channel::encode_one_frame( const AudioChannel& channel )
{
  enc_.encode( channel.region( cursor(), opus_frame::NUM_SAMPLES ), output_.writable_region().at( 0 ) );
  output_.push( 1 );
}

size_t OpusEncoderProcess::Channel::cursor() const
{
  return output_.num_pushed() * opus_frame::NUM_SAMPLES;
}

void OpusEncoderProcess::Channel::reset( const int bit_rate, const int sample_rate )
{
  enc_ = { bit_rate, sample_rate };
}

void OpusEncoderProcess::reset( const int bit_rate, const int sample_rate )
{
  enc1_.reset( bit_rate, sample_rate );
  enc2_.reset( bit_rate, sample_rate );
}
