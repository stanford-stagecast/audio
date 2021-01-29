#pragma once

#include "parser.hh"

static constexpr uint16_t control_port()
{
  return 3007;
}

template<uint8_t control_id>
struct control_message
{
  static constexpr uint8_t id = control_id;
};

struct set_cursor_lag : public control_message<0>
{
  uint16_t num_samples;

  constexpr uint32_t serialized_length() const { return sizeof( num_samples ); }
  void serialize( Serializer& s ) const { s.integer( num_samples ); }
  void parse( Parser& p ) { p.integer( num_samples ); }
};

struct set_loopback_gain : public control_message<1>
{
  float gain;

  constexpr uint32_t serialized_length() const { return sizeof( gain ); }
  void serialize( Serializer& s ) const { s.floating( gain ); }
  void parse( Parser& p ) { p.floating( gain ); }
};
