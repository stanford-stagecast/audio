#include "formats.hh"
#include "exception.hh"
#include "opus.hh"

using namespace std;

uint8_t AudioFrame::serialized_length() const
{
  return sizeof( frame_index ) + ch1.serialized_length() + ch2.serialized_length();
}

void AudioFrame::serialize( Serializer& s ) const
{
  s.integer( frame_index );
  s.object( ch1 );
  s.object( ch2 );
}

void AudioFrame::parse( Parser& p )
{
  p.integer( frame_index );
  p.object( ch1 );
  p.object( ch2 );
}

uint32_t Packet::serialized_length() const
{
  return sizeof( cumulative_ack ) + selective_acks.serialized_length() + frames.serialized_length();
}

void Packet::serialize( Serializer& s ) const
{
  s.integer( cumulative_ack );
  s.object( selective_acks );
  s.object( frames );
}

void Packet::parse( Parser& p )
{
  p.integer( cumulative_ack );
  p.object( selective_acks );
  p.object( frames );
}
