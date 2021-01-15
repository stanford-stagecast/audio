#include "keys.hh"

using namespace std;

LongLivedKey::LongLivedKey( const string_view name )
  : key_pair_()
  , name_( name )
{}

LongLivedKey::LongLivedKey( Parser& p )
  : key_pair_()
  , name_( "" )
{
  parse( p );
}

uint32_t LongLivedKey::serialized_length() const
{
  return key_pair_.serialized_length() + name_.serialized_length();
}

void LongLivedKey::serialize( Serializer& s ) const
{
  s.object( key_pair_ );
  s.object( name_ );
}

void LongLivedKey::parse( Parser& p )
{
  p.object( key_pair_ );
  p.object( name_ );

  /* failure to decode is fatal */
  if ( p.error() ) {
    p.clear_error();
    throw runtime_error( "failed to parse LongLivedKey" );
  }
}
