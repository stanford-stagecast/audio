#include "keys.hh"

using namespace std;

LongLivedKey::LongLivedKey( const string_view name )
  : encrypt_key_()
  , decrypt_key_()
  , name_( name )
{}

uint32_t LongLivedKey::serialized_length() const
{
  return 2 * sizeof( Base64Key::PrintableKey::printable_key ) + name_.serialized_length();
}

void LongLivedKey::serialize( Serializer& s ) const
{
  s.string( encrypt_key_.printable_key() );
  s.string( decrypt_key_.printable_key() );
  s.object( name_ );
}

void LongLivedKey::parse( Parser& p )
{
  array<char, sizeof( Base64Key::PrintableKey::printable_key )> keybuf;
  string_span keybuf_span { keybuf.data(), keybuf.size() };

  p.string( keybuf_span );
  encrypt_key_ = Base64Key( keybuf_span );

  p.string( keybuf_span );
  decrypt_key_ = Base64Key( keybuf_span );

  p.object( name_ );

  /* failure to decode is fatal */
  if ( p.error() ) {
    throw runtime_error( "failed to parse LongLivedKey" );
  }
}
