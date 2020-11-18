#pragma once

#include <array>
#include <string>

class Base64Key
{
private:
  alignas( 16 ) std::array<uint8_t, 16> key_;

public:
  Base64Key(); /* random key */
  Base64Key( const std::string_view printable_key );

  struct PrintableKey
  {
    std::array<char, 22> printable_key;
    operator std::string_view() const { return { printable_key.data(), printable_key.size() }; }
    std::string_view as_string_view() const { return static_cast<std::string_view>( *this ); }
  };

  PrintableKey printable_key() const;

  const std::array<uint8_t, 16>& key() const { return key_; }
};
