#pragma once

#include <array>
#include <memory>

#include "ae.hh"
#include "base64.hh"
#include "spans.hh"

class Nonce
{
private:
  std::array<char, 12> bytes_;

public:
  Nonce( const uint64_t value );
  Nonce( const std::string_view bytes );

  static constexpr uint8_t SERIALIZED_LEN = 8;

  std::string_view data() const { return { bytes_.data(), bytes_.size() }; }
  std::string_view lower64() const { return { bytes_.data() + 4, SERIALIZED_LEN }; }
  uint64_t value() const;
};

template<int max_len>
struct TextBuffer
{
  alignas( 16 ) std::array<char, max_len> buffer {};
  string_span data { buffer.data(), max_len };
  operator string_span() { return data; }
  operator std::string_view() const { return data; }
  size_t size() const { return data.size(); }
  void resize( const size_t size )
  {
    data = { buffer.data(), size };
    validate();
  }

  void validate() const;
};

using Plaintext = TextBuffer<1456>;
using Ciphertext = TextBuffer<1456 + 16 + 8 + 8>; /* + tag + nonce + AD */

class CryptoSession
{
  uint64_t nonce_val_;

  struct AEContext
  {
    static constexpr uint8_t TAG_LEN = 16;

    struct ae_deleter
    {
      void operator()( ae_ctx* x ) const;
    };

    std::unique_ptr<ae_ctx, ae_deleter> ctx;

    AEContext( const Base64Key& s_key );
    ~AEContext();
  } encrypt_ctx_, decrypt_ctx_;

public:
  CryptoSession( const Base64Key& encrypt_key, const Base64Key& decrypt_key );

  void encrypt( const std::string_view associated_data, const Plaintext& plaintext, Ciphertext& ciphertext );
  bool decrypt( const Ciphertext& ciphertext,
                const std::string_view expected_associated_data,
                Plaintext& plaintext ) const;
};
