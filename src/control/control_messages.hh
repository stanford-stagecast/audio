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
  NetString feed {};
  uint16_t target_samples {}, min_samples {}, max_samples {};

  uint32_t serialized_length() const
  {
    return name.serialized_length() + feed.serialized_length() + sizeof( min_samples ) + sizeof( target_samples )
           + sizeof( max_samples );
  }
  void serialize( Serializer& s ) const
  {
    s.object( feed );
    s.object( name );
    s.integer( target_samples );
    s.integer( min_samples );
    s.integer( max_samples );
  }
  void parse( Parser& p )
  {
    p.object( feed );
    p.object( name );
    p.integer( target_samples );
    p.integer( min_samples );
    p.integer( max_samples );
  }
};

struct set_gain : public control_message<1>
{
  NetString board_name {}, channel_name {};
  float gain1 {}, gain2 {};

  uint32_t serialized_length() const
  {
    return board_name.serialized_length() + channel_name.serialized_length() + sizeof( gain1 ) + sizeof( gain2 );
  }
  void serialize( Serializer& s ) const
  {
    s.object( board_name );
    s.object( channel_name );
    s.floating( gain1 );
    s.floating( gain2 );
  }
  void parse( Parser& p )
  {
    p.object( board_name );
    p.object( channel_name );
    p.floating( gain1 );
    p.floating( gain2 );
  }
};

struct set_live : public control_message<0>
{
  NetString name {};

  uint32_t serialized_length() const { return name.serialized_length(); }
  void serialize( Serializer& s ) const { s.object( name ); }
  void parse( Parser& p ) { p.object( name ); }
};
