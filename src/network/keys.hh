#pragma once

#include <string_view>

#include "base64.hh"
#include "formats.hh"
#include "parser.hh"

class LongLivedKey
{
  KeyPair key_pair_;
  NetString name_;

public:
  LongLivedKey( const std::string_view name );
  LongLivedKey( Parser& p );

  uint32_t serialized_length() const;
  void serialize( Serializer& s ) const;
  void parse( Parser& p );

  const std::string_view name() const { return name_; }
  const KeyPair& key_pair() const { return key_pair_; }
};
