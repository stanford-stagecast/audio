#include "http_writer.hh"

using namespace std;

class WriteAttempt
{
  RingBuffer& buffer_;
  size_t& bytes_written_;
  size_t remaining_offset_;
  bool completed_ { true };

public:
  WriteAttempt( RingBuffer& buffer, size_t& bytes_written )
    : buffer_( buffer )
    , bytes_written_( bytes_written )
    , remaining_offset_( bytes_written )
  {}

  void write( std::string_view str )
  {
    if ( remaining_offset_ >= str.size() ) {
      remaining_offset_ -= str.size();
      return;
    }

    if ( remaining_offset_ > 0 ) {
      str.remove_prefix( remaining_offset_ );
      remaining_offset_ = 0;
    }

    const size_t bytes_written_this_write = buffer_.push_from_const_str( str );
    if ( bytes_written_this_write != str.size() ) {
      completed_ = false;
    }
    bytes_written_ += bytes_written_this_write;
  }

  bool completed() const { return completed_; }

  void write_start_line( const HTTPRequest& request )
  {
    write( request.method );
    write( " " );
    write( request.request_target );
    write( " " );
    write( request.http_version );
    write( "\r\n" );
  }

  void write_start_line( const HTTPResponse& response )
  {
    write( response.http_version );
    write( " " );
    write( response.status_code );
    write( " " );
    write( response.reason_phrase );
    write( "\r\n" );
  }
};

template<class MessageType>
HTTPWriter<MessageType>::HTTPWriter( MessageType&& message )
  : message_( move( message ) )
{}

template<class MessageType>
void HTTPWriter<MessageType>::write_to( RingBuffer& buffer )
{
  WriteAttempt attempt { buffer, bytes_written_ };

  /* write start line (request or status ) */
  attempt.write_start_line( message_ );

  /* write headers */
  if ( message_.headers.content_length.has_value() ) {
    attempt.write( "Content-Length: " );
    attempt.write( to_string( message_.headers.content_length.value() ) );
    attempt.write( "\r\n" );
  }

  if ( not message_.headers.host.empty() ) {
    attempt.write( "Host: " );
    attempt.write( message_.headers.host );
    attempt.write( "\r\n" );
  }

  if ( message_.headers.connection_close ) {
    attempt.write( "Connection: close\r\n" );
  }

  /* end of headers */
  attempt.write( "\r\n" );

  /* write body */
  attempt.write( message_.body );

  if ( attempt.completed() ) {
    finished_ = true;
  }
}

template class HTTPWriter<HTTPRequest>;
template class HTTPWriter<HTTPResponse>;
