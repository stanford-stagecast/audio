#pragma once

#include "ring_buffer.hh"
#include "ws_frame.hh"

#include <optional>
#include <string_view>
#include <string>

class WebSocketServer
{
  bool message_in_progress_ {}, error_ {}, closed_ {};

  WebSocketFrame message_ {}, this_frame_ {};
  std::optional<WebSocketFrameReader> reader_ {};

  void send_pong();
  void send_close();

public:
  void send_all( const std::string_view serialized_frame, RingBuffer& out );

  size_t read( const std::string_view input );
  bool should_close_connection() const { return error_ or closed_; }

  void pop_message();
  bool ready() const { return message_.fin; }
  const std::string& message() const { return message_.payload; }
};
