#pragma once

#include <dbus/dbus.h>
#include <memory>
#include <optional>
#include <string>

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

  static std::optional<AudioDeviceClaim> try_claim( const std::string_view name );
};
