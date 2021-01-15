#pragma once

#include <array>
#include <string>

#include "parser.hh"

class Base64Key
{
private:
  static constexpr uint8_t KEY_LEN = 16;

  alignas( 16 ) std::array<uint8_t, KEY_LEN> key_;

public:
  Base64Key(); /* random key */
  Base64Key( const std::string_view printable_key );
  Base64Key( const std::array<uint8_t, KEY_LEN>& key )
    : key_( key )
  {}

  struct PrintableKey
  {
    std::array<char, 22> printable_key;
    operator std::string_view() const { return { printable_key.data(), printable_key.size() }; }
    std::string_view as_string_view() const { return static_cast<std::string_view>( *this ); }
  };

  PrintableKey printable_key() const;

  const std::array<uint8_t, KEY_LEN>& key() const { return key_; }

  static constexpr uint32_t serialized_length() { return KEY_LEN; }
  void serialize( Serializer& s ) const;
  void parse( Parser& p );
};

struct KeyPair
{
  Base64Key downlink {}, uplink {};

  static constexpr uint32_t serialized_length() { return 2 * Base64Key::serialized_length(); }
  void serialize( Serializer& s ) const
  {
    downlink.serialize( s );
    uplink.serialize( s );
  }
  void parse( Parser& p )
  {
    downlink.parse( p );
    uplink.parse( p );
  }
};
