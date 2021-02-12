#include "ws_server.hh"

using namespace std;

void WebSocketServer::send_all( const string_view serialized_frame, RingBuffer& out )
{
  if ( serialized_frame.size() > out.writable_region().size() ) {
    throw runtime_error( "send_all: no room to write" );
  }

  out.push_from_const_str( serialized_frame );
}

void WebSocketServer::pop_message()
{
  message_.clear();
  message_in_progress_ = false;
}

size_t WebSocketServer::read( const string_view input )
{
  if ( should_close_connection() ) {
    return 0;
  }

  if ( not reader_.has_value() ) {
    reader_.emplace( move( this_frame_ ) );
  }

  const size_t ret = reader_->read( input );
  if ( reader_->error() ) {
    error_ = true;
    reader_->clear_error();
    return ret;
  }

  if ( not reader_->finished() ) {
    return ret;
  }

  /* finished with frame */
  this_frame_ = reader_->release();
  reader_.reset();

  switch ( this_frame_.opcode ) {
    case WebSocketFrame::opcode_t::Ping:
      if ( not this_frame_.fin ) {
        error_ = true;
        return ret;
      }
      send_pong();
      break;
    case WebSocketFrame::opcode_t::Pong:
      if ( not this_frame_.fin ) {
        error_ = true;
        return ret;
      }
      break;
    case WebSocketFrame::opcode_t::Close:
      if ( not this_frame_.fin ) {
        error_ = true;
        return ret;
      }

      send_close();
      closed_ = true;
      break;
    case WebSocketFrame::opcode_t::Text:
    case WebSocketFrame::opcode_t::Binary:
      if ( message_in_progress_ ) {
        error_ = true;
        return ret;
      } else {
        message_ = move( this_frame_ );
        message_in_progress_ = true;
      }
      break;
    case WebSocketFrame::opcode_t::Continuation:
      if ( ( not message_in_progress_ ) or ( message_.fin ) ) {
        error_ = true;
        return ret;
      } else {
        message_.payload.append( this_frame_.payload );
        message_.fin = this_frame_.fin;
      }
      break;
  }

  return ret;
}
