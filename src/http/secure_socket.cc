#include "secure_socket.hh"
#include "exception.hh"

using namespace std;

void OpenSSL::check_errors( const std::string_view context, const bool must_have_error )
{
  unsigned long first_error = ERR_get_error();

  if ( first_error == 0 ) { // no error
    if ( must_have_error ) {
      throw runtime_error( "SSL error, but nothing on error queue" );
    } else {
      return;
    }
  }

  unsigned long next_error = ERR_get_error();

  if ( next_error == 0 ) {
    throw ssl_error( context, first_error );
  } else {
    string errors = ERR_error_string( first_error, nullptr );
    errors.append( ", " );
    errors.append( ERR_error_string( next_error, nullptr ) );

    while ( unsigned long another_error = ERR_get_error() ) {
      errors.append( ", " );
      errors.append( ERR_error_string( another_error, nullptr ) );
    }

    throw runtime_error( "multiple SSL errors: " + errors );
  }
}

SSLContext::SSLContext()
  : ctx_( SSL_CTX_new( TLS_client_method() ) )
{
  if ( not ctx_ ) {
    OpenSSL::throw_error( "SSL_CTX_new" );
  }

  SSL_CTX_set_mode( ctx_.get(), SSL_MODE_AUTO_RETRY );
  SSL_CTX_set_mode( ctx_.get(), SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER );
  SSL_CTX_set_mode( ctx_.get(), SSL_MODE_ENABLE_PARTIAL_WRITE );
  SSL_CTX_set_verify( ctx_.get(), SSL_VERIFY_PEER, nullptr );
  SSL_CTX_set1_cert_store( ctx_.get(), certificate_store_ );
}

void SSLContext::trust_certificate( const string_view cert_pem )
{
  Certificate cert { cert_pem };
  certificate_store_.add_certificate( cert );
}

SSL_handle SSLContext::make_SSL_handle()
{
  SSL_handle ssl { SSL_new( ctx_.get() ) };
  if ( not ssl ) {
    OpenSSL::throw_error( "SSL_new" );
  }
  return ssl;
}

int tcpsocket_BIO_write( BIO* bio, const char* const buf, const int len )
{
  TCPSocket* sock = reinterpret_cast<TCPSocket*>( BIO_get_data( bio ) );
  if ( not sock ) {
    throw runtime_error( "BIO_get_data returned nullptr" );
  }

  if ( len < 0 ) {
    throw runtime_error( "ringbuffer_BIO_write: len < 0" );
  }

  const size_t bytes_written = sock->write( { buf, static_cast<size_t>( len ) } );

  if ( bytes_written == 0 ) {
    BIO_set_retry_write( bio );
  }

  return bytes_written;
}

int tcpsocket_BIO_read( BIO* bio, char* const buf, const int len )
{
  TCPSocket* sock = reinterpret_cast<TCPSocket*>( BIO_get_data( bio ) );
  if ( not sock ) {
    throw runtime_error( "BIO_get_data returned nullptr" );
  }

  if ( len < 0 ) {
    throw runtime_error( "ringbuffer_BIO_write: len < 0" );
  }

  const size_t bytes_read = sock->read( { buf, static_cast<size_t>( len ) } );

  if ( bytes_read == 0 ) {
    BIO_set_retry_read( bio );
  }

  return bytes_read;
}

long tcpsocket_BIO_ctrl( BIO*, const int cmd, const long, void* const )
{
  if ( cmd == BIO_CTRL_FLUSH ) {
    return 1;
  }

  return 0;
}

BIO_METHOD* make_method( const string& name )
{
  const int new_type = BIO_get_new_index();
  if ( new_type == -1 ) {
    OpenSSL::throw_error( "BIO_get_new_index" );
  }

  BIO_METHOD* const ret = BIO_meth_new( new_type, name.c_str() );
  if ( not ret ) {
    OpenSSL::throw_error( "BIO_meth_new" );
  }

  return ret;
}

TCPSocketBIO::Method::Method()
  : method_( make_method( "TCPSocket" ) )
{
  if ( not BIO_meth_set_write( method_.get(), tcpsocket_BIO_write ) ) {
    OpenSSL::throw_error( "BIO_meth_set_write" );
  }

  if ( not BIO_meth_set_read( method_.get(), tcpsocket_BIO_read ) ) {
    OpenSSL::throw_error( "BIO_meth_set_read" );
  }

  if ( not BIO_meth_set_ctrl( method_.get(), tcpsocket_BIO_ctrl ) ) {
    OpenSSL::throw_error( "BIO_meth_set_ctrl" );
  }
}

const BIO_METHOD* TCPSocketBIO::Method::method()
{
  static Method method;
  return method.method_.get();
}

TCPSocketBIO::TCPSocketBIO( TCPSocket&& sock )
  : TCPSocket( move( sock ) )
  , bio_( BIO_new( Method::method() ) )
{
  if ( not bio_ ) {
    OpenSSL::throw_error( "BIO_new" );
  }

  BIO_set_data( bio_.get(), static_cast<TCPSocket*>( this ) );
  BIO_up_ref( bio_.get() );
  BIO_up_ref( bio_.get() );

  OpenSSL::check( "TCPSocketBIO constructor" );
}

MemoryBIO::MemoryBIO( const string_view contents )
  : contents_( move( contents ) )
  , bio_( BIO_new_mem_buf( contents.data(), contents.size() ) )
{
  if ( not bio_ ) {
    OpenSSL::throw_error( "BIO_new_mem_buf" );
  }
}

Certificate::Certificate( const string_view contents )
  : certificate_()
{
  MemoryBIO mem { contents };
  certificate_.reset( PEM_read_bio_X509( mem, nullptr, nullptr, nullptr ) );
  if ( not certificate_ ) {
    OpenSSL::throw_error( "PEM_read_bio_X509" );
  }
}

CertificateStore::CertificateStore()
  : certificate_store_( X509_STORE_new() )
{
  if ( not certificate_store_ ) {
    OpenSSL::throw_error( "X509_STORE_new" );
  }
}

void CertificateStore::add_certificate( Certificate& cert )
{
  if ( not X509_STORE_add_cert( certificate_store_.get(), cert ) ) {
    OpenSSL::throw_error( "X509_STORE_add_cert" );
  }
}

SSLSession::SSLSession( SSL_handle&& ssl, TCPSocket&& sock, const string& hostname )
  : ssl_( move( ssl ) )
  , socket_( move( sock ) )
{
  if ( not ssl_ ) {
    throw runtime_error( "SecureSocket: constructor must be passed valid SSL structure" );
  }

  SSL_set0_rbio( ssl_.get(), socket_ );
  SSL_set0_wbio( ssl_.get(), socket_ );

  if ( not SSL_set1_host( ssl_.get(), hostname.c_str() ) ) {
    OpenSSL::throw_error( "SSL_set1_host" );
  }

  SSL_set_connect_state( ssl_.get() );

  OpenSSL::check( "SSLSession constructor" );
}

int SSLSession::get_error( const int return_value ) const
{
  return SSL_get_error( ssl_.get(), return_value );
}

bool SSLSession::want_read() const
{
  return ( not read_waiting_on_write_ ) and ( not inbound_plaintext_.writable_region().empty() )
         and ( not incoming_stream_terminated_ );
}

bool SSLSession::want_write() const
{
  return ( not write_waiting_on_read_ ) and ( not outbound_plaintext_.readable_region().empty() );
}

void SSLSession::do_read()
{
  OpenSSL::check( "SSLSession::do_read()" );

  auto target = inbound_plaintext_.writable_region();

  const auto read_count_before = socket_.read_count();
  const int bytes_read = SSL_read( ssl_.get(), target.mutable_data(), target.size() );
  const auto read_count_after = socket_.read_count();

  if ( read_count_after > read_count_before or bytes_read > 0 ) {
    write_waiting_on_read_ = false;
  }

  if ( bytes_read > 0 ) {
    inbound_plaintext_.push( bytes_read );
    return;
  }

  const int error_return = get_error( bytes_read );

  if ( bytes_read == 0 and error_return == SSL_ERROR_ZERO_RETURN ) {
    incoming_stream_terminated_ = true;
    return;
  }

  if ( error_return == SSL_ERROR_WANT_WRITE ) {
    read_waiting_on_write_ = true;
    return;
  }

  if ( error_return == SSL_ERROR_WANT_READ ) {
    return;
  }

  OpenSSL::check( "SSL_read check" );
  throw ssl_error( "SSL_read", error_return );
}

void SSLSession::do_write()
{
  OpenSSL::check( "SSLSession::do_write()" );

  const string_view source = outbound_plaintext_.readable_region();

  const auto write_count_before = socket_.write_count();
  const int bytes_written = SSL_write( ssl_.get(), source.data(), source.size() );
  const auto write_count_after = socket_.write_count();

  if ( write_count_after > write_count_before or bytes_written > 0 ) {
    read_waiting_on_write_ = false;
  }

  if ( bytes_written > 0 ) {
    outbound_plaintext_.pop( bytes_written );
    return;
  }

  const int error_return = get_error( bytes_written );

  if ( error_return == SSL_ERROR_WANT_READ ) {
    write_waiting_on_read_ = true;
    return;
  }

  if ( error_return == SSL_ERROR_WANT_WRITE ) {
    return;
  }

  OpenSSL::check( "SSL_write check" );
  throw ssl_error( "SSL_write", error_return );
}
