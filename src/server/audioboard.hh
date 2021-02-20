#pragma once

#include <vector>

#include "audio_buffer.hh"
#include "encoder_task.hh"
#include "webmwriter.hh"

class AudioBoard
{
  std::vector<std::pair<std::string, AudioChannel>> channels_ {};

public:
  AudioBoard( const uint8_t num_channels );

  void set_name( const uint8_t ch_num, const std::string_view name ) { channels_.at( ch_num ).first = name; }

  const AudioChannel& channel( const uint8_t ch_num ) const { return channels_.at( ch_num ).second; }
  AudioChannel& channel( const uint8_t ch_num ) { return channels_.at( ch_num ).second; }

  void pop_samples_until( const uint64_t sample );

  uint8_t num_channels() const { return channels_.size(); }
  const std::string& channel_name( const uint8_t num ) const { return channels_.at( num ).first; }
};

class AudioWriter
{
  ChannelPair mixed_audio_ { 8192 };

  uint64_t mix_cursor_ {};

  OpusEncoderProcess encoder_ { 96000, 48000 };
  WebMWriter webm_writer_ { 96000, 48000, 2 };

public:
  void mix_and_write( const AudioBoard& board, const uint64_t cursor_sample );
};
