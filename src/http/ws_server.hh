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

  void read( RingBuffer& in, RingBuffer& out );
  bool should_close_connection() const { return error_ or closed_; }

  void pop_message();
  bool ready() const { return message_.fin; }
  const std::string& message() const { return message_.payload; }
};

class WebSocketServer
{
  std::string origin_ {};

  HTTPRequestReader handshake_reader_ { {}, {} };

  bool error_ {};
  bool handshake_complete_ {};

  WebSocketEndpoint wse_ {};

  void send_forbidden_response( RingBuffer& out );

public:
  WebSocketServer( const std::string_view origin )
    : origin_( origin )
  {}

  bool handshake_complete() const { return handshake_complete_; }
  WebSocketEndpoint& endpoint() { return wse_; }

  void do_handshake( RingBuffer& in, RingBuffer& out );

  bool should_close_connection() const { return error_ or wse_.should_close_connection(); }
};
