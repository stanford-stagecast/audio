#include "ws_server.hh"

#include <crypto++/base64.h>
#include <crypto++/hex.h>
#include <crypto++/sha.h>

using namespace std;
using namespace CryptoPP;

void WebSocketEndpoint::pop_message()
{
  message_.clear();
  message_in_progress_ = false;
}

void WebSocketEndpoint::send_pong( RingBuffer& out )
{
  this_frame_.opcode = WebSocketFrame::opcode_t::Pong;
  this_frame_.masking_key.reset();

  if ( out.writable_region().size() >= this_frame_.serialized_length() ) {
    Serializer s { out.writable_region() };
    s.object( this_frame_ );
    out.push( s.bytes_written() );
  }
}

void WebSocketEndpoint::send_close( RingBuffer& out )
{
  this_frame_.clear();
  this_frame_.fin = true;
  this_frame_.opcode = WebSocketFrame::opcode_t::Close;

  if ( out.writable_region().size() >= this_frame_.serialized_length() ) {
    Serializer s { out.writable_region() };
    s.object( this_frame_ );
    out.push( s.bytes_written() );
  }
}

void WebSocketEndpoint::read( RingBuffer& in, RingBuffer& out )
{
  if ( should_close_connection() ) {
    return;
  }

  if ( not reader_.has_value() ) {
    reader_.emplace( move( this_frame_ ) );
  }

  in.pop( reader_->read( in.readable_region() ) );
  if ( reader_->error() ) {
    error_ = true;
    reader_->clear_error();
    return;
  }

  if ( not reader_->finished() ) {
    return;
  }

  /* finished with frame */
  this_frame_ = reader_->release();
  reader_.reset();

  switch ( this_frame_.opcode ) {
    case WebSocketFrame::opcode_t::Ping:
      if ( not this_frame_.fin ) {
        error_ = true;
        return;
      }
      send_pong( out );
      break;
    case WebSocketFrame::opcode_t::Pong:
      if ( not this_frame_.fin ) {
        error_ = true;
        return;
      }
      break;
    case WebSocketFrame::opcode_t::Close:
      if ( not this_frame_.fin ) {
        error_ = true;
        return;
      }

      send_close( out );
      closed_ = true;
      break;
    case WebSocketFrame::opcode_t::Text:
    case WebSocketFrame::opcode_t::Binary:
      if ( message_in_progress_ ) {
        error_ = true;
        return;
      } else {
        message_ = move( this_frame_ );
        message_in_progress_ = true;
      }
      break;
    case WebSocketFrame::opcode_t::Continuation:
      if ( ( not message_in_progress_ ) or ( message_.fin ) ) {
        error_ = true;
        return;
      } else {
        message_.payload.append( this_frame_.payload );
        message_.fin = this_frame_.fin;
      }
      break;
  }
}

static constexpr char WS_MAGIC_STRING[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

void WebSocketServer::do_handshake( RingBuffer& in, RingBuffer& out )
{
  cerr << "do_handshake called\n";

  if ( handshake_complete() ) {
    throw runtime_error( "do_handshake: handshake is complete" );
  }

  if ( not handshake_reader_.finished() ) {
    in.pop( handshake_reader_.read( in.readable_region() ) );
  }

  if ( not handshake_reader_.finished() ) {
    cerr << "still not finished\n";
    return;
  }

  const auto request = handshake_reader_.release();

  if ( request.method != "GET" ) {
    cerr << "not get\n";
    send_forbidden_response( out );
    return;
  }

  if ( request.http_version != "HTTP/1.1" ) {
    cerr << "not HTTP/1.1\n";
    send_forbidden_response( out );
    return;
  }

  /*
  if ( not request.headers.connection_upgrade ) {
    cerr << "no connection upgrade\n";
    send_forbidden_response( out );
    return;
  }
  */

  if ( not HTTPHeaderReader::header_equals( request.headers.upgrade, "websocket" ) ) {
    cerr << "no upgrade websocket: header value is {" + request.headers.upgrade + "}\n";
    send_forbidden_response( out );
    return;
  }

  if ( request.headers.sec_websocket_key.empty() ) {
    cerr << "no key\n";
    send_forbidden_response( out );
    return;
  }

  if ( request.headers.origin != origin_ ) {
    cerr << "bad origin (" + request.headers.origin + " vs. " + origin_ + "\n";
    send_forbidden_response( out );
    return;
  }

  cerr << "Sending happy response\n";

  /* make reply */
  HTTPResponse response;
  response.http_version = "HTTP/1.1";
  response.status_code = "101";
  response.reason_phrase = "Switching Protocols";
  response.headers.connection = "upgrade";
  response.headers.upgrade = "websocket";

  CryptoPP::SHA1 sha1_function;
  StringSource s(
    request.headers.sec_websocket_key + WS_MAGIC_STRING,
    true,
    new HashFilter( sha1_function,
                    new Base64Encoder( new StringSink( response.headers.sec_websocket_accept ), false ) ) );

  HTTPResponse response_save = response;

  HTTPResponseWriter writer { move( response ) };
  writer.write_to( out );
  if ( not writer.finished() ) { /* XXX assume there is enough space */
    error_ = true;
    return;
  }
  handshake_complete_ = true;

  cerr << "Here is the happy response:\n";

  HTTPResponseWriter writer2 { move( response_save ) };
  RingBuffer debug_out { 16384 };
  writer2.write_to( debug_out );
  cerr << debug_out.readable_region() << "\n\n";
}

void WebSocketServer::send_forbidden_response( RingBuffer& out )
{
  cerr << "Sending forbidden response\n";

  error_ = true;

  HTTPResponse response;
  response.http_version = "HTTP/1.1";
  response.status_code = "403";
  response.reason_phrase = "Forbidden";

  HTTPResponseWriter writer { move( response ) };
  writer.write_to( out );
}

void WebSocketEndpoint::send_all( const string_view serialized_frame, RingBuffer& out )
{
  if ( out.writable_region().size() < serialized_frame.size() ) {
    throw runtime_error( "no room to send_all" );
  }

  out.push_from_const_str( serialized_frame );
}
