#include "encoder_task.hh"

using namespace std;

template class EncoderTask<AudioDeviceTask>;

template<class AudioSource>
EncoderTask<AudioSource>::EncoderTask( const AudioType audio_type,
                                       const int bit_rate,
                                       const int sample_rate,
                                       const shared_ptr<AudioSource> source,
                                       EventLoop& loop )
  : OpusEncoderProcess( audio_type, bit_rate, sample_rate )
  , source_( source )
{
  loop.add_rule(
    "encode",
    [&] {
      enc1_.encode_one_frame( source_->capture().ch1() );
      pop_from_source();
    },
    [&] { return enc1_.can_encode_frame( source_->cursor() ); } );
}

template<class AudioSource>
EncoderTask<AudioSource>::EncoderTask( const AudioType audio_type,
                                       const int bit_rate1,
                                       const int bit_rate2,
                                       const int sample_rate,
                                       const shared_ptr<AudioSource> source,
                                       EventLoop& loop )
  : OpusEncoderProcess( audio_type, bit_rate1, bit_rate2, sample_rate )
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
      enc2_.value().encode_one_frame( source_->capture().ch2() );
      pop_from_source();
    },
    [&] { return enc2_.value().can_encode_frame( source_->cursor() ); } );
}

template<class AudioSource>
void EncoderTask<AudioSource>::pop_from_source()
{
  const auto encode_cursor = min_encode_cursor();
  if ( encode_cursor > source_->capture().range_begin() ) {
    source_->capture().pop_before( encode_cursor );
  }
}

bool OpusEncoderProcess::has_frame() const
{
  if ( not enc1_.output().has_value() ) {
    return false;
  }

  if ( not enc2_.has_value() ) {
    return true;
  }

  return enc2_.value().output().has_value();
}

void OpusEncoderProcess::pop_frame()
{
  if ( not has_frame() ) {
    throw std::runtime_error( "pop_frame() but not has_frame()" );
  }
  enc1_.output().reset();
  if ( enc2_.has_value() ) {
    enc2_->output().reset();
  }
  num_popped_++;
}

OpusEncoderProcess::OpusEncoderProcess( const AudioType audio_type,
                                        const int bit_rate1,
                                        const int bit_rate2,
                                        const int sample_rate )
  : audio_type_( audio_type )
  , enc1_( bit_rate1, sample_rate )
  , enc2_( make_optional<TrackedEncoder>( bit_rate2, sample_rate ) )
{
  if ( audio_type_ != Audio::TwoChannel ) {
    throw runtime_error( "use of two-channel constructor without TwoChannel AudioType" );
  }
}

OpusEncoderProcess::OpusEncoderProcess( const AudioType audio_type, const int bit_rate1, const int sample_rate )
  : audio_type_( audio_type )
  , enc1_( bit_rate1, sample_rate )
  , enc2_()
{
  if ( audio_type_ == Audio::TwoChannel ) {
    throw runtime_error( "use of TwoChannel AudioType without two-channel constructor" );
  }
}

OpusEncoderProcess::TrackedEncoder::TrackedEncoder( const int bit_rate, const int sample_rate )
  : enc_( bit_rate, sample_rate, 1, OPUS_APPLICATION_RESTRICTED_LOWDELAY )
{}

size_t OpusEncoderProcess::min_encode_cursor() const
{
  if ( enc2_.has_value() ) {
    return min( enc1_.cursor(), enc2_->cursor() );
  } else {
    return enc1_.cursor();
  }
}

bool OpusEncoderProcess::TrackedEncoder::can_encode_frame( const size_t source_cursor ) const
{
  return ( source_cursor >= cursor() + opus_frame::NUM_SAMPLES_MINLATENCY ) and ( not output_.has_value() );
}

void OpusEncoderProcess::TrackedEncoder::encode_one_frame( const AudioChannel& channel )
{
  if ( output_.has_value() ) {
    throw runtime_error( "internal error: encode_one_frame called but output already has value" );
  }

  output_.emplace();
  enc_.encode( channel.region( cursor(), opus_frame::NUM_SAMPLES_MINLATENCY ), output_.value() );
  num_pushed_++;
}

void OpusEncoderProcess::TrackedEncoder::reset( const int bit_rate, const int sample_rate )
{
  enc_ = { bit_rate, sample_rate, 1, OPUS_APPLICATION_RESTRICTED_LOWDELAY };
}

void OpusEncoderProcess::reset( const int bit_rate1, const int sample_rate )
{
  enc1_.reset( bit_rate1, sample_rate );
}

void OpusEncoderProcess::reset( const int bit_rate1, const int bit_rate2, const int sample_rate )
{
  enc1_.reset( bit_rate1, sample_rate );
  enc2_.value().reset( bit_rate2, sample_rate );
}

void OpusEncoderProcess::encode_one_frame( const AudioChannel& ch1, const AudioChannel& ch2 )
{
  enc1_.encode_one_frame( ch1 );
  enc2_.value().encode_one_frame( ch2 );
}
