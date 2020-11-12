#pragma once

#include <alsa/asoundlib.h>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "file_descriptor.hh"
#include "typed_ring_buffer.hh"

class ALSADevices
{
public:
  struct Device
  {
    std::string name;
    std::vector<std::pair<std::string, std::string>> interfaces;
  };

  static std::vector<Device> list();

  static std::pair<std::string, std::string> find_device( const std::string_view expected_description );
};

class PCMFD : public FileDescriptor
{
public:
  using FileDescriptor::FileDescriptor;

  using FileDescriptor::register_read;
};

class AudioBuffer
{
  EndlessBuffer<float> ch1, ch2;

public:
  AudioBuffer( const size_t capacity )
    : ch1( capacity )
    , ch2( capacity )
  {}

  size_t range_begin() const { return ch1.range_begin(); }
  size_t range_end() const { return ch1.range_end(); }

  std::pair<span<float>, span<float>> write_region( const size_t pos, const size_t count )
  {
    return { ch1.region( pos, count ), ch2.region( pos, count ) };
  }

  std::pair<span_view<float>, span_view<float>> read_region( const size_t pos, const size_t count ) const
  {
    return { ch1.region( pos, count ), ch2.region( pos, count ) };
  }

  void pop( const size_t num_samples )
  {
    ch1.pop( num_samples );
    ch2.pop( num_samples );
  }

  std::pair<float, float> safe_get( const size_t index ) const
  {
    if ( index < range_begin() or index >= range_end() ) {
      return {};
    }

    const auto x = read_region( index, 1 );
    return { x.first[0], x.second[0] };
  }

  void safe_set( const size_t index, const std::pair<float, float> val )
  {
    if ( index < range_begin() or index >= range_end() ) {
      return;
    }

    auto x = write_region( index, 1 );
    std::tie( x.first[0], x.second[0] ) = val;
  }
};

struct AudioStatistics
{
  unsigned int recoveries;

  unsigned int total_wakeups;
  unsigned int empty_wakeups;

  unsigned int max_microphone_avail;
  unsigned int min_headphone_delay { std::numeric_limits<unsigned int>::max() };
  unsigned int max_combined_samples;

  struct SampleStats
  {
    unsigned int samples_skipped;
    float max_ch1_amplitude, max_ch2_amplitude;
  } sample_stats;
};

class AudioInterface
{
  std::string interface_name_, annotation_;
  snd_pcm_t* pcm_;

  void check_state( const snd_pcm_state_t expected_state );

  snd_pcm_sframes_t avail_ {}, delay_ {};

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
                                      AudioBuffer& capture_output,
                                      size_t& capture_index,
                                      const AudioBuffer& playback_input,
                                      size_t& playback_index,
                                      AudioStatistics::SampleStats& stats );

  const Configuration& config() const { return config_; }
  void set_config( const Configuration& other ) { config_ = other; }

  snd_pcm_state_t state() const;
  unsigned int avail() const { return avail_; }
  unsigned int delay() const { return delay_; }

  std::string name() const;
  PCMFD fd();

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
  void loopback( AudioBuffer& capture_output,
                 size_t& capture_index,
                 const AudioBuffer& playback_input,
                 size_t& playback_index );

  unsigned int mic_avail() { return microphone_.avail(); }

  const AudioStatistics& statistics() { return statistics_; }
  void reset_statistics()
  {
    auto rec = statistics_.recoveries, skip = statistics_.sample_stats.samples_skipped;
    statistics_ = {};
    statistics_.recoveries = rec;
    statistics_.sample_stats.samples_skipped = skip;
  }
};

inline float float_to_dbfs( const float sample_f )
{
  return 20 * log10( sample_f );
}
