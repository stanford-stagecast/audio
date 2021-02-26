#pragma once

#include "http_reader.hh"
#include "parser.hh"

#include <array>
#include <cstdint>
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
  std::optional<std::array<uint8_t, 4>> masking_key {};
  std::string payload {};

  void serialize( std::string& out ) const;
  void serialize( Serializer& s ) const;
  uint32_t serialized_length() const;

  bool operator==( const WebSocketFrame& other ) const
  {
    return fin == other.fin and opcode == other.opcode and masking_key == other.masking_key
           and payload == other.payload;
  }

  bool operator!=( const WebSocketFrame& other ) const { return not operator==( other ); }

  void clear();

  static constexpr uint8_t max_overhead() { return 14; }
};

template<size_t target_length>
class ArrayReader
{
  std::array<uint8_t, target_length> target_ {};
  size_t length_so_far_ {};

public:
  using Contained = std::array<uint8_t, target_length>;
  const Contained& value() { return target_; }

  std::string_view as_string_view() const
  {
    return { reinterpret_cast<const char*>( target_.data() ), target_length };
  }

  bool finished() const { return length_so_far_ == target_length; }

  size_t read( const std::string_view input )
  {
    const std::string_view readable_portion = input.substr( 0, target_length - length_so_far_ );
    memcpy( target_.data() + length_so_far_, readable_portion.data(), readable_portion.size() );
    length_so_far_ += readable_portion.size();
    return readable_portion.size();
  }
};

class WebSocketFrameReader
{
  WebSocketFrame target_;

  bool error_ {};

  ArrayReader<2> bytes12_ {};

  std::optional<ArrayReader<2>> len16_reader_ {};
  std::optional<ArrayReader<8>> len64_reader_ {};
  std::optional<ArrayReader<4>> masking_key_reader_ {};

  std::optional<LengthReader> payload_reader_ {};

  bool finished_ {};

  void process_bytes12();
  void process_len16();
  void process_len64();
  void apply_mask();

  void complete();

public:
  WebSocketFrame release() { return std::move( target_ ); }

  WebSocketFrameReader( WebSocketFrame&& target )
    : target_( std::move( target ) )
  {
    target_.clear();
  }

  ~WebSocketFrameReader()
  {
    if ( error_ ) {
      std::cerr << "Error: WebSocketFrameReader destroyed without clearing error.\n";
      abort();
    }
  }

  void clear_error() { error_ = false; }
  bool error() const { return error_; }
  bool finished() const { return finished_; }

  size_t read( const std::string_view orig_input );
};
