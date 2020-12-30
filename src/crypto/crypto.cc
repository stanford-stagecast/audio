#include "crypto.hh"
#include "base64.hh"
#include "exception.hh"

#include <cstring>
#include <iostream>
#include <unistd.h>

using namespace std;

void CryptoSession::AEContext::ae_deleter::operator()( ae_ctx* x ) const
{
  ae_free( x );
}

CryptoSession::AEContext::AEContext( const Base64Key& key )
  : ctx( ae_allocate( nullptr ) )
{
  if ( AE_SUCCESS != ae_init( ctx.get(), key.key().data(), 16, 12, TAG_LEN ) ) {
    throw runtime_error( "Could not initialize AE context" );
  }
}

CryptoSession::AEContext::~AEContext()
{
  if ( ae_clear( ctx.get() ) != AE_SUCCESS ) {
    cerr << "Error clearing AE context.\n";
  }
}

CryptoSession::CryptoSession( const Base64Key& encrypt_key, const Base64Key& decrypt_key )
  : nonce_val_()
  , encrypt_ctx_( encrypt_key )
  , decrypt_ctx_( decrypt_key )
{
  CheckSystemCall( "getentropy", getentropy( &nonce_val_, sizeof( nonce_val_ ) ) ); /* XXX avoid reuse of nonce */
}

template<int max_len>
void TextBuffer<max_len>::validate() const
{
  if ( data.length() > max_len ) {
    throw runtime_error( "TextBuffer length is invalid: " + to_string( data.length() ) + " > "
                         + to_string( max_len ) );
  }
}

Nonce::Nonce( const uint64_t value )
  : bytes_()
{
  memset( bytes_.data(), 0, 4 );
  memcpy( bytes_.data() + 4, &value, 8 );
}

Nonce::Nonce( const string_view bytes )
  : bytes_()
{
  if ( bytes.size() != SERIALIZED_LEN ) {
    throw std::runtime_error( "invalid nonce length" );
  }

  memset( bytes_.data(), 0, 4 );
  memcpy( bytes_.data() + 4, bytes.data(), 8 );
}

uint64_t Nonce::value() const
{
  uint64_t ret = 0;
  memcpy( &ret, bytes_.data() + 4, 8 );
  return ret;
}

void CryptoSession::encrypt( const string_view associated_data, const Plaintext& plaintext, Ciphertext& ciphertext )
{
  plaintext.validate();

  Nonce nonce { nonce_val_++ };
  const int ciphertext_len = plaintext.size() + AEContext::TAG_LEN;

  ciphertext.resize( ciphertext_len + Nonce::SERIALIZED_LEN + associated_data.size() );

  memcpy( ciphertext.buffer.data() + ciphertext_len, nonce.lower64().data(), Nonce::SERIALIZED_LEN );
  memcpy( ciphertext.buffer.data() + ciphertext_len + Nonce::SERIALIZED_LEN,
          associated_data.data(),
          associated_data.size() );

  if ( ciphertext_len
       != ae_encrypt( encrypt_ctx_.ctx.get(),   /* ctx */
                      nonce.data().data(),      /* nonce */
                      plaintext.buffer.data(),  /* pt */
                      plaintext.size(),         /* pt_len */
                      associated_data.data(),   /* ad */
                      associated_data.size(),   /* ad_len */
                      ciphertext.buffer.data(), /* ct */
                      nullptr,                  /* tag */
                      AE_FINALIZE ) ) {         /* final */
    throw runtime_error( "ae_encrypt() returned error" );
  }
}

bool CryptoSession::decrypt( const Ciphertext& ciphertext,
                             const string_view expected_associated_data,
                             Plaintext& plaintext ) const
{
  ciphertext.validate();

  if ( ciphertext.size() < AEContext::TAG_LEN + Nonce::SERIALIZED_LEN + expected_associated_data.size() ) {
    return false;
  }

  const int body_len = ciphertext.size() - Nonce::SERIALIZED_LEN - expected_associated_data.size();

  const int pt_len = body_len - AEContext::TAG_LEN;
  plaintext.resize( pt_len );

  Nonce nonce { static_cast<string_view>( ciphertext ).substr( body_len, Nonce::SERIALIZED_LEN ) };

  if ( pt_len
       != ae_decrypt( decrypt_ctx_.ctx.get(),          /* ctx */
                      nonce.data().data(),             /* nonce */
                      ciphertext.buffer.data(),        /* ct */
                      body_len,                        /* ct_len */
                      expected_associated_data.data(), /* ad */
                      expected_associated_data.size(), /* ad_len */
                      plaintext.buffer.data(),         /* pt */
                      nullptr,                         /* tag */
                      AE_FINALIZE ) ) {                /* final */
    return false;
  }

  const string_view actual_associated_data { static_cast<string_view>( ciphertext )
                                               .substr( body_len + Nonce::SERIALIZED_LEN,
                                                        expected_associated_data.size() ) };
  if ( actual_associated_data != expected_associated_data ) {
    throw runtime_error( "associated data mismatch" );
  }

  return true;
}
