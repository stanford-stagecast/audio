#pragma once

#include "formats.hh"
#include "parser.hh"

static constexpr uint16_t client_control_port()
{
  return 3007;
}

static constexpr uint16_t server_control_port()
{
  return 3008;
}

static constexpr uint16_t video_client_control_port()
{
  return 4007;
}

static constexpr uint16_t video_server_control_port()
{
  return 4008;
}

template<uint8_t control_id>
struct control_message
{
  static constexpr uint8_t id = control_id;
};

struct set_cursor_lag : public control_message<0>
{
  NetString name {};
  uint16_t num_samples {};

  uint32_t serialized_length() const { return name.serialized_length() + sizeof( num_samples ); }
  void serialize( Serializer& s ) const
  {
    s.object( name );
    s.integer( num_samples );
  }
  void parse( Parser& p )
  {
    p.object( name );
    p.integer( num_samples );
  }
};

struct set_gain : public control_message<1>
{
  NetString name {};
  float gain {};

  uint32_t serialized_length() const { return name.serialized_length() + sizeof( gain ); }
  void serialize( Serializer& s ) const
  {
    s.object( name );
    s.floating( gain );
  }
  void parse( Parser& p )
  {
    p.object( name );
    p.floating( gain );
  }
};

struct set_live : public control_message<0>
{
  NetString name {};

  uint32_t serialized_length() const { return name.serialized_length(); }
  void serialize( Serializer& s ) const { s.object( name ); }
  void parse( Parser& p ) { p.object( name ); }
};
