#include <alsa/asoundlib.h>
#include <dbus/dbus.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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

class AudioInterface
{
  std::string interface_name_, annotation_;
  snd_pcm_t* pcm_;

  void check_state( const snd_pcm_state_t expected_state );

public:
  AudioInterface( const std::string_view interface_name,
                  const std::string_view annotation,
                  const snd_pcm_stream_t stream );

  void configure();

  std::string name() const;

  ~AudioInterface();

  /* can't copy or assign */
  AudioInterface( const AudioInterface& other ) = delete;
  AudioInterface& operator=( const AudioInterface& other ) = delete;
};
