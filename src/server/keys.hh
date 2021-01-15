#pragma once

#include <string_view>

#include "base64.hh"
#include "formats.hh"
#include "parser.hh"

class LongLivedKey
{
  Base64Key encrypt_key_, decrypt_key_;
  NetString<32> name_;

public:
  LongLivedKey( const std::string_view name );
  LongLivedKey( Parser& p );

  uint32_t serialized_length() const;
  void serialize( Serializer& s ) const;
  void parse( Parser& p );

  const std::string& name() const { return name_; }
};
