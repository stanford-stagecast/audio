#pragma once

#include <array>
#include <memory>

#include "ae.hh"
#include "base64.hh"
#include "spans.hh"
#include "stackbuffer.hh"

class Nonce
{
public:
  static constexpr uint8_t SERIALIZED_LEN = 8;
  static constexpr uint8_t INTERNAL_LEN = 12;

private:
  std::array<char, INTERNAL_LEN> bytes_;

public:
  Nonce( const uint64_t value );
  Nonce( const std::string_view bytes );

  std::string_view data() const { return { bytes_.data(), bytes_.size() }; }
  std::string_view lower64() const { return { bytes_.data() + 4, SERIALIZED_LEN }; }
  uint64_t value() const;
};

using Plaintext = StackBuffer<16, 1456>;
using Ciphertext = StackBuffer<16, 1456 + 16 + 8 + 8>; /* + tag + nonce + AD */

class CryptoSession
{
  bool randomize_nonce_ {};
  uint64_t nonce_val_;
  uint64_t blocks_encrypted_ {};

  void set_random_nonce();

  struct ae_deleter
  {
    void operator()( ae_ctx* x ) const noexcept;
  };

  std::unique_ptr<ae_ctx, ae_deleter> encrypt_context_, decrypt_context_;

public:
  static constexpr uint8_t TAG_LEN = 16;

  CryptoSession( const Base64Key& encrypt_key, const Base64Key& decrypt_key, const bool randomize_nonce = false );

  void encrypt( const std::string_view associated_data, const Plaintext& plaintext, Ciphertext& ciphertext );

  bool decrypt( const Ciphertext& ciphertext,
                const std::string_view expected_associated_data,
                Plaintext& plaintext ) const;

  CryptoSession( const CryptoSession& other ) = delete;
  CryptoSession& operator=( const CryptoSession& other ) = delete;

  CryptoSession( CryptoSession&& other );
};
