#pragma once

#include "http_server.hh"
#include "ring_buffer.hh"
#include "ws_frame.hh"

#include <optional>
#include <string>
#include <string_view>

class WebSocketEndpoint
{
  bool message_in_progress_ {}, error_ {}, closed_ {};

  WebSocketFrame message_ {}, this_frame_ {};
  std::optional<WebSocketFrameReader> reader_ {};

  void send_pong( RingBuffer& out );
  void send_close( RingBuffer& out );

public:
  void send_all( const std::string_view serialized_frame, RingBuffer& out );

  size_t read( const std::string_view input, RingBuffer& out );
  bool should_close_connection() const { return error_ or closed_; }

  void pop_message();
  bool ready() const { return message_.fin; }
  const std::string& message() const { return message_.payload; }
};

class WebSocketServer
{
  std::string origin_ {};

  HTTPRequestReader handshake_reader_ { {}, {} };
  std::optional<HTTPResponseWriter> handshake_writer_ {};

  bool error_ {};

  WebSocketEndpoint wse_ {};

public:
  WebSocketServer( const std::string_view origin )
    : origin_( origin )
  {}

  bool handshake_complete() { return handshake_writer_.has_value() and handshake_writer_->finished(); }
  WebSocketEndpoint endpoint() { return wse_; }

  void do_handshake( RingBuffer& in, RingBuffer& out );

  bool should_close_connection() const { return error_ or wse_.should_close_connection(); }
};
