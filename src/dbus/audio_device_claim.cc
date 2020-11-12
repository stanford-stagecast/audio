#include <iostream>
#include <limits>

#include "audio_device_claim.hh"
#include "exception.hh"

using namespace std;

struct DBusMem_deleter
{
  void operator()( char* x ) const { dbus_free( x ); }
};
using DBusString = unique_ptr<char, DBusMem_deleter>;

struct DBusMessage_deleter
{
  void operator()( DBusMessage* x ) const { dbus_message_unref( x ); }
};
using DBusMessageWrapper = unique_ptr<DBusMessage, DBusMessage_deleter>;

class DBusMethodCall
{
  DBusMessageWrapper message_;
  DBusMessageIter argument_iterator_;

public:
  DBusMethodCall( const char* destination, const char* path, const char* iface, const char* method )
    : message_(
      notnull( "dbus_message_new_method_call", dbus_message_new_method_call( destination, path, iface, method ) ) )
    , argument_iterator_()
  {
    dbus_message_iter_init_append( message_.get(), &argument_iterator_ );
  }

  void add_argument( const char* str )
  {
    if ( not dbus_message_iter_append_basic( &argument_iterator_, DBUS_TYPE_STRING, &str ) ) {
      throw bad_alloc {};
    }
  }

  void add_argument( const int32_t num )
  {
    if ( not dbus_message_iter_append_basic( &argument_iterator_, DBUS_TYPE_INT32, &num ) ) {
      throw bad_alloc {};
    }
  }

  operator DBusMessage*() { return message_.get(); }

  DBusMethodCall( const DBusMethodCall& other ) = delete;
  DBusMethodCall* operator=( DBusMethodCall& other ) = delete;
};

class DBusErrorWrapper
{
  DBusError error_;

public:
  DBusErrorWrapper()
    : error_()
  {
    dbus_error_init( &error_ );
  }

  ~DBusErrorWrapper() { dbus_error_free( &error_ ); }
  operator DBusError*() { return &error_; }
  bool is_error() const { return dbus_error_is_set( &error_ ); }
  string to_string() const { return error_.name + ": "s + error_.message; }
  void throw_if_error() const
  {
    if ( is_error() ) {
      throw runtime_error( to_string() );
    }
  }

  DBusErrorWrapper( const DBusErrorWrapper& other ) = delete;
  DBusErrorWrapper* operator=( DBusErrorWrapper& other ) = delete;
};

void AudioDeviceClaim::DBusConnectionWrapper::DBusConnection_deleter::operator()( DBusConnection* x ) const
{
  dbus_connection_unref( x );
}

AudioDeviceClaim::DBusConnectionWrapper::DBusConnectionWrapper( const DBusBusType type )
  : connection_( [&] {
    DBusErrorWrapper error;
    DBusConnection* ret = dbus_bus_get( type, error );
    error.throw_if_error();
    notnull( "dbus_bus_get", ret );
    return ret;
  }() )
{
  dbus_connection_set_exit_on_disconnect( connection_.get(), false );
}

AudioDeviceClaim::DBusConnectionWrapper::operator DBusConnection*()
{
  return connection_.get();
}

AudioDeviceClaim::AudioDeviceClaim( const string_view name )
  : connection_( DBUS_BUS_SESSION )
{
  const string resource = "org.freedesktop.ReserveDevice1." + string( name );
  const string path = "/org/freedesktop/ReserveDevice1/" + string( name );

  {
    DBusErrorWrapper error;
    const int ret = dbus_bus_request_name( connection_, resource.c_str(), DBUS_NAME_FLAG_DO_NOT_QUEUE, error );
    error.throw_if_error();
    if ( ret == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER ) {
      /* success -- was not already claimed */
      //      cerr << "Successfully claimed uncontested device " << name << ".\n";
      return;
    }
  }

  {
    DBusMethodCall method { resource.c_str(), path.c_str(), "org.freedesktop.DBus.Properties", "Get" };
    method.add_argument( "org.freedesktop.ReserveDevice1" );
    method.add_argument( "ApplicationName" );

    {
      DBusErrorWrapper error;
      DBusMessageWrapper reply { dbus_connection_send_with_reply_and_block( connection_, method, 1000, error ) };
      error.throw_if_error();
      notnull( "dbus_connection_send_with_reply_and_block", reply.get() );

      DBusMessageIter iterator;
      if ( not dbus_message_iter_init( reply.get(), &iterator ) ) {
        throw runtime_error( "no arguments" );
      }

      if ( DBUS_TYPE_VARIANT != dbus_message_iter_get_arg_type( &iterator ) ) {
        throw runtime_error( "unexpected argument type (not variant)" );
      }

      const DBusString variant_type { dbus_message_iter_get_signature( &iterator ) };
      if ( string( variant_type.get() ) != DBUS_TYPE_VARIANT_AS_STRING ) {
        throw runtime_error( "unexpected argument type (not variant as string)" );
      }

      DBusMessageIter iterator2;
      dbus_message_iter_recurse( &iterator, &iterator2 );

      if ( DBUS_TYPE_STRING != dbus_message_iter_get_arg_type( &iterator2 ) ) {
        throw runtime_error( "unexpected argument type (recursed into variant but did not find string)" );
      }

      char* strptr;
      dbus_message_iter_get_basic( &iterator2, &strptr );

      claimed_from_.emplace( strptr );
    }
  }

  // claim it
  {
    DBusMethodCall method { resource.c_str(), path.c_str(), "org.freedesktop.ReserveDevice1", "RequestRelease" };
    method.add_argument( numeric_limits<int32_t>::max() );

    {
      DBusErrorWrapper error;
      DBusMessageWrapper reply { dbus_connection_send_with_reply_and_block( connection_, method, 1000, error ) };
      error.throw_if_error();
      notnull( "dbus_connection_send_with_reply_and_block", reply.get() );

      DBusMessageIter iterator;
      if ( not dbus_message_iter_init( reply.get(), &iterator ) ) {
        throw runtime_error( "no arguments" );
      }

      if ( DBUS_TYPE_BOOLEAN != dbus_message_iter_get_arg_type( &iterator ) ) {
        throw runtime_error( "unexpected argument type (not bool)" );
      }

      bool result = 0;
      dbus_message_iter_get_basic( &iterator, &result );
    }
  }

  {
    DBusErrorWrapper error;
    const int ret = dbus_bus_request_name(
      connection_, resource.c_str(), DBUS_NAME_FLAG_DO_NOT_QUEUE | DBUS_NAME_FLAG_REPLACE_EXISTING, error );
    error.throw_if_error();
    if ( ret == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER ) {
      //      cerr << "Successfully claimed contested device " << name << ".\n";
    } else {
      string error_message = "Could not claim device " + string( name );
      if ( claimed_from_.has_value() ) {
        error_message += " from " + claimed_from_.value();
      }
      throw runtime_error( error_message );
    }
  }
}

optional<AudioDeviceClaim> AudioDeviceClaim::try_claim( const string_view name )
{
  try {
    AudioDeviceClaim ownership { name };

    cerr << "Claimed ownership of " << name;
    if ( ownership.claimed_from() ) {
      cerr << " from " << ownership.claimed_from().value();
    }
    cerr << endl;

    return ownership;
  } catch ( const exception& e ) {
    cerr << "Failed to claim ownership: " << e.what() << "\n";
    return {};
  }
}
