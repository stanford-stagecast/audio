#include <dbus/dbus.h>
#include <memory>
#include <string>
#include <vector>

class ALSADevices
{
public:
  struct Device
  {
    std::string name;
    std::vector<std::pair<std::string, std::string>> outputs;
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

  DBusConnectionWrapper dbus_connection_;

public:
  AudioDeviceClaim( const std::string_view name );
};
