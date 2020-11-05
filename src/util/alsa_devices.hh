#pragma once

#include <alsa/asoundlib.h>
#include <dbus/dbus.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "socket.hh"
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
};

class AudioDeviceClaim
{
  class DBusConnectionWrapper
  {
    struct DBusConnection_deleter
    {
      void operator()( DBusConnection* x ) const;
    };
    std::unique_ptr<DBusConnection, DBusConnection_deleter> connection_;

  public:
    DBusConnectionWrapper( const DBusBusType type );
    operator DBusConnection*();
  };

  DBusConnectionWrapper connection_;

  std::optional<std::string> claimed_from_ {};

public:
  AudioDeviceClaim( const std::string_view name );

  const std::optional<std::string>& claimed_from() const { return claimed_from_; }
};

class PCMFD : public FileDescriptor
{
public:
  using FileDescriptor::FileDescriptor;

  using FileDescriptor::register_read;
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

  unsigned int read_from_socket();
  unsigned int copy_all_available_samples_to( AudioInterface& other );

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

  struct Statistics
  {
    unsigned int recoveries;
    unsigned int samples_skipped;

    unsigned int total_wakeups;
    unsigned int empty_wakeups;

    unsigned int max_microphone_avail;
    unsigned int min_headphone_delay { std::numeric_limits<unsigned int>::max() };
    unsigned int max_combined_samples;
  };

  Statistics statistics_ {};

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
  void loopback();

  const Statistics& statistics() { return statistics_; }
  void reset_statistics()
  {
    auto rec = statistics_.recoveries, skip = statistics_.samples_skipped;
    statistics_ = {};
    statistics_.recoveries = rec;
    statistics_.samples_skipped = skip;
  }
};
