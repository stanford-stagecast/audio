#pragma once

#include <vector>

#include "audio_buffer.hh"
#include "encoder_task.hh"
#include "socket.hh"

#include <json/json.h>

class AudioBoard
{
  std::string name_;
  std::vector<std::pair<std::string, AudioChannel>> channels_ {};
  std::vector<std::pair<float, float>> gains_ {};
  std::vector<float> power_ {};

public:
  AudioBoard( const std::string_view name, const uint8_t num_channels );

  const std::string& name() const { return name_; }

  void set_gain( const std::string_view channel_name, const float gain1, const float gain2 );

  void set_channel_name( const uint8_t ch_num, const std::string_view name )
  {
    channels_.at( ch_num ).first = name;
  }

  const AudioChannel& channel( const uint8_t ch_num ) const { return channels_.at( ch_num ).second; }
  AudioChannel& channel( const uint8_t ch_num ) { return channels_.at( ch_num ).second; }

  void pop_samples_until( const uint64_t sample );

  uint8_t num_channels() const { return channels_.size(); }
  const std::string& channel_name( const uint8_t num ) const { return channels_.at( num ).first; }

  const std::pair<float, float>& gain( const uint8_t ch_num ) const { return gains_.at( ch_num ); }

  void json_summary( Json::Value& root, const bool include_second_channels ) const;
};

class AudioWriter
{
  ChannelPair mixed_audio_ { 8192 };

  uint64_t mix_cursor_ {};

  OpusEncoder encoder_ { 96000, 48000, 2, OPUS_APPLICATION_AUDIO };

  Address destination_;
  UnixDatagramSocket socket_ {};

public:
  AudioWriter( const std::string_view socket_path );
  void mix_and_write( const AudioBoard& board, const uint64_t cursor_sample );
};
