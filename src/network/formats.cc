#include "formats.hh"
#include "exception.hh"
#include "opus.hh"

using namespace std;

uint8_t AudioFrame::serialized_length() const
{
  return sizeof( frame_index ) + frame1.serialized_length()
         + ( separate_channels ? frame2.serialized_length() : 0 );
}

void AudioFrame::serialize( Serializer& s ) const
{
  const uint32_t first_word = ( separate_channels << 31 ) | ( frame_index & 0x7FFF'FFFF );

  s.integer( first_word );
  s.object( frame1 );

  if ( separate_channels ) {
    s.object( frame2 );
  }
}

void AudioFrame::parse( Parser& p )
{
  uint32_t first_word {};
  p.integer( first_word );
  frame_index = first_word & 0x7FFF'FFFF;
  separate_channels = first_word & 0x8000'0000;

  p.object( frame1 );

  if ( separate_channels ) {
    p.object( frame2 );
  }
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

void KeyMessage::serialize( Serializer& s ) const
{
  s.object( id );
  s.object( key_pair );
}

void KeyMessage::parse( Parser& p )
{
  p.object( id );
  p.object( key_pair );
}
