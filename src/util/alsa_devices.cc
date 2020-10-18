#include <alsa/asoundlib.h>
#include <cstdlib>
#include <dbus/dbus.h>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "alsa_devices.hh"

using namespace std;

template<typename T>
inline T* notnull( const string_view context, T* const x )
{
  return x ? x : throw runtime_error( string( context ) + ": returned null pointer" );
}

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

class DBusConnectionWrapper
{
  struct DBusConnection_deleter
  {
    void operator()( DBusConnection* x ) const { dbus_connection_unref( x ); }
  };

  unique_ptr<DBusConnection, DBusConnection_deleter> connection_;

public:
  DBusConnectionWrapper( const DBusBusType type )
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

  operator DBusConnection*() { return connection_.get(); }
};

class alsa_error_category : public error_category
{
public:
  const char* name() const noexcept override { return "alsa_error_category"; }
  string message( const int return_value ) const noexcept override { return snd_strerror( return_value ); }
};

class alsa_error : public system_error
{
  string what_;

public:
  alsa_error( const string& context, const int err )
    : system_error( err, alsa_error_category() )
    , what_( context + ": " + system_error::what() )
  {}

  const char* what() const noexcept override { return what_.c_str(); }
};

int alsa_check( const char* context, const int return_value )
{
  if ( return_value >= 0 ) {
    return return_value;
  }
  throw alsa_error( context, return_value );
}

int alsa_check( const string& context, const int return_value )
{
  return alsa_check( context.c_str(), return_value );
}

vector<ALSADevices::Device> ALSADevices::list()
{
  int card = -1;

  struct ALSA_hint_deleter
  {
    void operator()( void** x ) const { snd_device_name_free_hint( x ); }
  };

  struct free_deleter
  {
    void operator()( void* x ) const { free( x ); }
  };

  vector<Device> ret;

  while ( true ) {
    alsa_check( "snd_card_next", snd_card_next( &card ) );
    if ( card < 0 ) {
      break;
    }

    ret.push_back( { "Audio" + to_string( card ), {} } );

    unique_ptr<void*, ALSA_hint_deleter> hints { [&] {
      void** hints_tmp;
      alsa_check( "snd_device_name_hint", snd_device_name_hint( card, "pcm", &hints_tmp ) );
      return hints_tmp;
    }() };

    for ( auto current_hint = hints.get(); *current_hint; ++current_hint ) {
      const unique_ptr<char, free_deleter> name { snd_device_name_get_hint( *current_hint, "NAME" ) };
      const unique_ptr<char, free_deleter> desc { snd_device_name_get_hint( *current_hint, "DESC" ) };

      if ( name and desc ) {
        const string_view name_str { name.get() }, desc_str { desc.get() };
        if ( name_str.substr( 0, 3 ) == "hw:"sv ) {
          const auto first_line = desc_str.substr( 0, desc_str.find_first_of( '\n' ) );
          ret.back().outputs.emplace_back( name_str, first_line );
        }
      }
    }
  }

  return ret;
}

#if 0
void program_body()
{
  list_alsa_devices();

  DBusConnectionWrapper connection { DBUS_BUS_SESSION };

  {
    DBusErrorWrapper error;
    const int ret = dbus_bus_request_name(
      connection, "org.freedesktop.ReserveDevice1.Audio1", DBUS_NAME_FLAG_DO_NOT_QUEUE, error );
    error.throw_if_error();
    if ( ret == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER ) {
      cout << "Success.\n";
      return;
    }
  }

  cout << "Failure -- currently in use.\n";

  {
    DBusMethodCall method { "org.freedesktop.ReserveDevice1.Audio1",
                            "/org/freedesktop/ReserveDevice1/Audio1",
                            "org.freedesktop.DBus.Properties",
                            "Get" };
    method.add_argument( "org.freedesktop.ReserveDevice1" );
    method.add_argument( "ApplicationName" );

    {
      DBusErrorWrapper error;
      DBusMessageWrapper reply { dbus_connection_send_with_reply_and_block( connection, method, 1000, error ) };
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

      cout << "ApplicationName: " << strptr << "\n";
    }
  }

  // claim it
  {
    DBusMethodCall method { "org.freedesktop.ReserveDevice1.Audio1",
                            "/org/freedesktop/ReserveDevice1/Audio1",
                            "org.freedesktop.ReserveDevice1",
                            "RequestRelease" };
    method.add_argument( numeric_limits<int32_t>::max() );

    {
      DBusErrorWrapper error;
      DBusMessageWrapper reply { dbus_connection_send_with_reply_and_block( connection, method, 1000, error ) };
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

      cout << "Result: " << result << "\n";
    }
  }

  {
    DBusErrorWrapper error;
    const int ret = dbus_bus_request_name( connection,
                                           "org.freedesktop.ReserveDevice1.Audio1",
                                           DBUS_NAME_FLAG_DO_NOT_QUEUE | DBUS_NAME_FLAG_REPLACE_EXISTING,
                                           error );
    error.throw_if_error();
    if ( ret == DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER ) {
      cout << "Success.\n";
    } else {
      throw runtime_error( "Could not claim device " + "Audio1"s );
    }
  }

  this_thread::sleep_for( chrono::seconds( 10 ) );
}

int main()
{
  try {
    program_body();
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
#endif
