#include "encoder_task.hh"
#include "audio_task.hh"

using namespace std;

template<class AudioSource>
OpusEncoderTask<AudioSource>::OpusEncoderTask( const int bit_rate,
                                               const int sample_rate,
                                               const shared_ptr<AudioSource> source,
                                               EventLoop& loop )
  : enc1_( bit_rate, sample_rate )
  , enc2_( bit_rate, sample_rate )
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
void OpusEncoderTask<AudioSource>::pop_from_source()
{
  const size_t min_encode_cursor = min( enc1_.cursor(), enc2_.cursor() );
  if ( min_encode_cursor > source_->capture().range_begin() ) {
    source_->capture().pop( min_encode_cursor - source_->capture().range_begin() );
  }
}

OpusEncoderProcess::OpusEncoderProcess( const int bit_rate, const int sample_rate )
  : enc_( bit_rate, sample_rate )
{}

bool OpusEncoderProcess::can_encode_frame( const size_t source_cursor ) const
{
  return ( source_cursor > cursor() + samples_per_frame ) and ( output_.writable_region().size() > 0 );
}

void OpusEncoderProcess::encode_one_frame( const AudioChannel& channel )
{
  enc_.encode( channel.region( cursor(), samples_per_frame ), output_.writable_region().at( 0 ) );
  output_.push( 1 );
}

size_t OpusEncoderProcess::cursor() const
{
  return output_.num_pushed() * samples_per_frame;
}

void OpusEncoderProcess::reset( const int bit_rate, const int sample_rate )
{
  enc_ = { bit_rate, sample_rate };
}

template<class AudioSource>
void OpusEncoderTask<AudioSource>::reset( const int bit_rate, const int sample_rate )
{
  enc1_.reset( bit_rate, sample_rate );
  enc2_.reset( bit_rate, sample_rate );
}

template class OpusEncoderTask<AudioDeviceTask>;
