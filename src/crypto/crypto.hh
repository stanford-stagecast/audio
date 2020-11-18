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

  std::string_view data() const { return { bytes_.data(), bytes_.size() }; }
  std::string_view lower64() const { return { bytes_.data() + 4, 8 }; }
  uint64_t value() const;
};

template<int max_len>
struct TextBuffer
{
  alignas( 16 ) std::array<char, max_len> buffer {};
  string_span data { buffer.data(), max_len };
  operator string_span() { return data; }
  size_t size() const { return data.size(); }
  void resize( const size_t size )
  {
    data = { buffer.data(), size };
    validate();
  }

  void validate() const;
};

using Plaintext = TextBuffer<2048>;
using Ciphertext = TextBuffer<2048 + 16 + 8>;

class Session
{
private:
  uint64_t nonce_val = 0;

  struct AEContext
  {
    struct ae_deleter
    {
      void operator()( ae_ctx* x ) const;
    };

    std::unique_ptr<ae_ctx, ae_deleter> ctx;

    AEContext( const Base64Key& s_key );
    ~AEContext();
  } encrypt_ctx_, decrypt_ctx_;

  mutable unsigned int decryption_failures_ {};

public:
  Session( const Base64Key& encrypt_key, const Base64Key& decrypt_key );

  void encrypt( const Plaintext& plaintext, Ciphertext& ciphertext );
  bool decrypt( const Ciphertext& ciphertext, Plaintext& plaintext ) const;

  unsigned int decryption_failures() const { return decryption_failures_; }
};
