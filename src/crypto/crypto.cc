#include "crypto.hh"
#include "base64.hh"
#include "exception.hh"

#include <cstring>
#include <iostream>
#include <unistd.h>

using namespace std;

void Session::AEContext::ae_deleter::operator()( ae_ctx* x ) const
{
  ae_free( x );
}

Session::AEContext::AEContext( const Base64Key& key )
  : ctx( ae_allocate( nullptr ) )
{
  if ( AE_SUCCESS != ae_init( ctx.get(), key.key().data(), 16, 12, 16 ) ) {
    throw runtime_error( "Could not initialize AE context" );
  }
}

Session::AEContext::~AEContext()
{
  if ( ae_clear( ctx.get() ) != AE_SUCCESS ) {
    cerr << "Error clearing AE context.\n";
  }
}

Session::Session( const Base64Key& encrypt_key, const Base64Key& decrypt_key )
  : nonce_val()
  , encrypt_ctx_( encrypt_key )
  , decrypt_ctx_( decrypt_key )
{
  CheckSystemCall( "getentropy", getentropy( &nonce_val, sizeof( nonce_val ) ) ); /* XXX avoid reuse of nonce */
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
  if ( bytes.size() != 8 ) {
    throw std::runtime_error( "invalid nonce length" );
  }

  memset( bytes_.data(), 0, 4 );
  memcpy( bytes_.data() + 4, bytes.data(), 8 );
}

uint64_t Nonce::value() const
{
  uint64_t ret;
  memcpy( &ret, bytes_.data() + 4, 8 );
  return ret;
}

void Session::encrypt( const Plaintext& plaintext, Ciphertext& ciphertext )
{
  plaintext.validate();

  Nonce nonce { nonce_val++ };
  const int ciphertext_len = plaintext.size() + 16;

  ciphertext.resize( ciphertext_len + nonce.lower64().size() );

  memcpy( ciphertext.buffer.data() + ciphertext_len, nonce.lower64().data(), nonce.lower64().size() );

  if ( ciphertext_len
       != ae_encrypt( encrypt_ctx_.ctx.get(),   /* ctx */
                      nonce.data().data(),      /* nonce */
                      plaintext.buffer.data(),  /* pt */
                      plaintext.size(),         /* pt_len */
                      nullptr,                  /* ad */
                      0,                        /* ad_len */
                      ciphertext.buffer.data(), /* ct */
                      nullptr,                  /* tag */
                      AE_FINALIZE ) ) {         /* final */
    throw runtime_error( "ae_encrypt() returned error" );
  }
}

bool Session::decrypt( const Ciphertext& ciphertext, Plaintext& plaintext ) const
{
  ciphertext.validate();

  if ( ciphertext.size() < 24 ) {
    return false;
  }

  const int body_len = ciphertext.size() - 8;
  const int pt_len = body_len - 16;
  plaintext.resize( pt_len );

  Nonce nonce { { ciphertext.data.end() - 8, 8 } };

  if ( pt_len
       != ae_decrypt( decrypt_ctx_.ctx.get(),   /* ctx */
                      nonce.data().data(),      /* nonce */
                      ciphertext.buffer.data(), /* ct */
                      body_len,                 /* ct_len */
                      nullptr,                  /* ad */
                      0,                        /* ad_len */
                      plaintext.buffer.data(),  /* pt */
                      nullptr,                  /* tag */
                      AE_FINALIZE ) ) {         /* final */
    return false;
  }

  return true;
}
