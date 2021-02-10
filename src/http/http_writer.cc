#include "http_writer.hh"

using namespace std;

HTTPRequestWriter::HTTPRequestWriter( HTTPRequest&& request )
  : request_( move( request ) )
{}

HTTPRequestWriter::WriteAttempt::WriteAttempt( RingBuffer& buffer, size_t& bytes_written )
  : buffer_( buffer )
  , bytes_written_( bytes_written )
  , remaining_offset_( bytes_written )
{}

void HTTPRequestWriter::WriteAttempt::write( std::string_view str )
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

void HTTPRequestWriter::write_to( RingBuffer& buffer )
{
  WriteAttempt attempt { buffer, bytes_written_ };

  /* write request line */
  attempt.write( request_.method );
  attempt.write( " " );
  attempt.write( request_.request_target );
  attempt.write( " " );
  attempt.write( request_.http_version );
  attempt.write( "\r\n" );

  /* write headers */
  if ( request_.headers.content_length.has_value() ) {
    attempt.write( "Content-Length: " );
    attempt.write( to_string( request_.headers.content_length.value() ) );
    attempt.write( "\r\n" );
  }

  if ( not request_.headers.host.empty() ) {
    attempt.write( "Host: " );
    attempt.write( request_.headers.host );
    attempt.write( "\r\n" );
  }

  if ( request_.headers.connection_close ) {
    attempt.write( "Connection: close\r\n" );
  }

  /* end of headers */
  attempt.write( "\r\n" );

  /* write body */
  attempt.write( request_.body );

  if ( attempt.completed() ) {
    finished_ = true;
  }
}
