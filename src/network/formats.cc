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
  return sizeof( sender_section.sequence_number ) + sender_section.frames.serialized_length()
         + sizeof( receiver_section.next_frame_needed ) + receiver_section.packets_received.serialized_length();
}

void Packet::serialize( Serializer& s ) const
{
  s.integer( sender_section.sequence_number );
  s.object( sender_section.frames );

  s.integer( receiver_section.next_frame_needed );
  s.object( receiver_section.packets_received );
}

void Packet::parse( Parser& p )
{
  p.integer( sender_section.sequence_number );
  p.object( sender_section.frames );

  p.integer( receiver_section.next_frame_needed );
  p.object( receiver_section.packets_received );
}

Packet::Record Packet::SenderSection::to_record() const
{
  Record ret;

  ret.sequence_number = sequence_number;
  ret.frames.length = frames.length;
  for ( uint8_t i = 0; i < frames.length; i++ ) {
    ret.frames.elements[i].value = frames.elements[i].frame_index;
  }

  return ret;
}
