#pragma once

#include "parser.hh"

#include <cstdint>
#include <optional>
#include <string>

struct WebSocketFrame
{
  enum class opcode_t : uint8_t
  {
    Continuation = 0x0,
    Text = 0x1,
    Binary = 0x2,
    Close = 0x8,
    Ping = 0x9,
    Pong = 0xA
  };

  bool fin {};
  opcode_t opcode {};
  std::optional<uint32_t> masking_key {};
  std::string payload {};

  uint32_t serialized_length() const;
  void serialize( Serializer& s ) const;
  void parse( Parser& p );
};
