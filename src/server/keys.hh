#pragma once

#include <string_view>

#include "base64.hh"
#include "formats.hh"
#include "parser.hh"

class LongLivedKey
{
  Base64Key encrypt_key_, decrypt_key_;
  NetArray<NetInteger<uint8_t>, 64> name_;

public:
  LongLivedKey( const std::string_view name );

  uint32_t serialized_length() const;
  void serialize( Serializer& s ) const;
  void parse( Parser& p );
};
