#pragma once

#include <alsa/asoundlib.h>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "audio_buffer.hh"
#include "file_descriptor.hh"

class ALSADevices
{
public:
  struct Device
  {
    std::string name;
    std::vector<std::pair<std::string, std::string>> interfaces;
  };

  static std::vector<Device> list();

  static std::pair<std::string, std::string> find_device( const std::vector<std::string_view> descriptions );
};

class PCMFD : public FileDescriptor
{
public:
  using FileDescriptor::FileDescriptor;

  using FileDescriptor::register_read;
};

struct AudioStatistics
{
  size_t last_recovery;
  unsigned int recoveries;

  unsigned int total_wakeups;
  unsigned int empty_wakeups;

  unsigned int max_microphone_avail;
  unsigned int min_headphone_delay { std::numeric_limits<unsigned int>::max() };
  unsigned int max_combined_samples;

  struct SampleStats
  {
    unsigned int samples_counted;
    unsigned int samples_skipped;
    float max_ch1_amplitude, max_ch2_amplitude;
    float ssa_ch1, ssa_ch2;
  } sample_stats;
};

class AudioInterface
{
  std::string interface_name_, annotation_;
  snd_pcm_t* pcm_;

  void check_state( const snd_pcm_state_t expected_state );

  snd_pcm_sframes_t avail_ {}, delay_ {};

  size_t cursor_ {};

  class Buffer
  {
    snd_pcm_t* pcm_;
    const snd_pcm_channel_area_t* areas_;
    unsigned int frame_count_;
    snd_pcm_uframes_t offset_;

  public:
    Buffer( AudioInterface& interface, const unsigned int sample_count );
    void commit( const unsigned int num_frames );
    void commit() { commit( frame_count_ ); }
    ~Buffer();

    int32_t& sample( const bool right_channel, const unsigned int sample_num )
    {
      return *( static_cast<int32_t*>( areas_[0].addr ) + right_channel + 2 * ( offset_ + sample_num ) );
    }

    /* can't copy or assign */
    Buffer( const Buffer& other ) = delete;
    Buffer& operator=( const Buffer& other ) = delete;

    unsigned int frame_count() const { return frame_count_; }
  };

public:
  struct Configuration
  {
    unsigned int sample_rate { 48000 };
    unsigned int avail_minimum { 6 };
    unsigned int period_size { 12 };
    unsigned int buffer_size { 192 };

    unsigned int start_threshold { 24 };
    unsigned int skip_threshold { 64 };

    std::array<float, 2> ch1_loopback_gain { 2.0, 2.0 };
    std::array<float, 2> ch2_loopback_gain { 2.0, 2.0 };
  };

private:
  Configuration config_ {};

public:
  AudioInterface( const std::string_view interface_name,
                  const std::string_view annotation,
                  const snd_pcm_stream_t stream );

  void initialize();
  void start();
  void prepare();
  void drop();
  void recover();
  bool update();

  void copy_all_available_samples_to( AudioInterface& other,
                                      ChannelPair& capture_output,
                                      const ChannelPair& playback_input,
                                      AudioStatistics::SampleStats& stats );

  const Configuration& config() const { return config_; }
  void set_config( const Configuration& other ) { config_ = other; }

  snd_pcm_state_t state() const;
  unsigned int avail() const { return avail_; }
  unsigned int delay() const { return delay_; }

  std::string name() const;
  PCMFD fd();

  size_t cursor() const { return cursor_; }

  ~AudioInterface();

  void link_with( AudioInterface& other );

  /* can't copy or assign */
  AudioInterface( const AudioInterface& other ) = delete;
  AudioInterface& operator=( const AudioInterface& other ) = delete;
};

class AudioPair
{
  AudioInterface headphone_, microphone_;
  AudioInterface::Configuration config_ {};
  PCMFD fd_ { microphone_.fd() };

  AudioStatistics statistics_ {};

public:
  AudioPair( const std::string_view interface_name );

  const AudioInterface::Configuration& config() const { return config_; }
  void set_config( const AudioInterface::Configuration& config );

  void initialize()
  {
    microphone_.initialize();
    headphone_.initialize();
  }

  PCMFD& fd() { return fd_; }

  void start() { microphone_.start(); }
  void recover();
  void loopback( ChannelPair& capture_output, const ChannelPair& playback_input );

  bool mic_has_samples();
  unsigned int mic_avail() { return microphone_.avail(); }

  size_t cursor() const { return microphone_.cursor(); }

  const AudioStatistics& statistics() const { return statistics_; }
  void reset_statistics()
  {
    const auto rec = statistics_.recoveries, skip = statistics_.sample_stats.samples_skipped;
    const auto last_recovery = statistics_.last_recovery;
    statistics_ = {};
    statistics_.recoveries = rec;
    statistics_.sample_stats.samples_skipped = skip;
    statistics_.last_recovery = last_recovery;
  }
};

inline float float_to_dbfs( const float sample_f )
{
  if ( sample_f <= 0.00001 ) {
    return -100;
  }

  return 20 * log10( sample_f );
}

inline float dbfs_to_float( const float dbfs )
{
  if ( dbfs <= -99 ) {
    return 0.0;
  }

  return exp10( dbfs / 20 );
}
