#include <alsa/asoundlib.h>
#include <cstdlib>
#include <dbus/dbus.h>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <thread>

#include "alsa_devices.hh"

using namespace std;
using namespace std::chrono;

template<typename T>
inline T* notnull( const char* context, T* const x )
{
  return x ? x : throw runtime_error( string( context ) + ": returned null pointer" );
}

template<typename T>
inline T* notnull( const string& context, T* const x )
{
  return notnull( context.c_str(), x );
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

#define alsa_check_easy( expr ) alsa_check( #expr, expr )

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
    alsa_check_easy( snd_card_next( &card ) );
    if ( card < 0 ) {
      break;
    }

    ret.push_back( { "Audio" + to_string( card ), {} } );

    unique_ptr<void*, ALSA_hint_deleter> hints { [&] {
      void** hints_tmp;
      alsa_check_easy( snd_device_name_hint( card, "pcm", &hints_tmp ) );
      return hints_tmp;
    }() };

    for ( auto current_hint = hints.get(); *current_hint; ++current_hint ) {
      const unique_ptr<char, free_deleter> name { snd_device_name_get_hint( *current_hint, "NAME" ) };
      const unique_ptr<char, free_deleter> desc { snd_device_name_get_hint( *current_hint, "DESC" ) };

      if ( name and desc ) {
        const string_view name_str { name.get() }, desc_str { desc.get() };
        if ( name_str.substr( 0, 3 ) == "hw:"sv ) {
          const auto first_line = desc_str.substr( 0, desc_str.find_first_of( '\n' ) );
          ret.back().interfaces.emplace_back( name_str, first_line );
        }
      }
    }
  }

  return ret;
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

AudioInterface::AudioInterface( const string_view interface_name,
                                const string_view annotation,
                                const snd_pcm_stream_t stream )
  : interface_name_( interface_name )
  , annotation_( annotation )
  , pcm_( nullptr )
  , buffer_size_( 0 )
{
  const string diagnostic = "snd_pcm_open(" + name() + ")";
  alsa_check( diagnostic, snd_pcm_open( &pcm_, interface_name_.c_str(), stream, SND_PCM_NONBLOCK ) );
  notnull( diagnostic, pcm_ );

  check_state( SND_PCM_STATE_OPEN );

  /* set desired hardware parameters */
  {
    struct params_deleter
    {
      void operator()( snd_pcm_hw_params_t* x ) const { snd_pcm_hw_params_free( x ); }
    };
    unique_ptr<snd_pcm_hw_params_t, params_deleter> params { [] {
      snd_pcm_hw_params_t* x = nullptr;
      snd_pcm_hw_params_malloc( &x );
      return notnull( "snd_pcm_hw_params_malloc", x );
    }() };

    alsa_check_easy( snd_pcm_hw_params_any( pcm_, params.get() ) );

    alsa_check_easy( snd_pcm_hw_params_set_rate_resample( pcm_, params.get(), false ) );
    alsa_check_easy( snd_pcm_hw_params_set_access( pcm_, params.get(), SND_PCM_ACCESS_MMAP_INTERLEAVED ) );
    alsa_check_easy( snd_pcm_hw_params_set_format( pcm_, params.get(), SND_PCM_FORMAT_S32_LE ) );
    alsa_check_easy( snd_pcm_hw_params_set_channels( pcm_, params.get(), 2 ) );
    alsa_check_easy( snd_pcm_hw_params_set_rate( pcm_, params.get(), 48000, 0 ) );
    alsa_check_easy( snd_pcm_hw_params_set_period_size( pcm_, params.get(), 24, 0 ) );

    /* apply hardware parameters */
    alsa_check_easy( snd_pcm_hw_params( pcm_, params.get() ) );

    /* save buffer size */
    alsa_check_easy( snd_pcm_hw_params_get_buffer_size( params.get(), &buffer_size_ ) );
  }

  check_state( SND_PCM_STATE_PREPARED );

  /* set desired software parameters */
  {
    struct params_deleter
    {
      void operator()( snd_pcm_sw_params_t* x ) const { snd_pcm_sw_params_free( x ); }
    };
    unique_ptr<snd_pcm_sw_params_t, params_deleter> params { [] {
      snd_pcm_sw_params_t* x = nullptr;
      snd_pcm_sw_params_malloc( &x );
      return notnull( "snd_pcm_sw_params_malloc", x );
    }() };

    alsa_check_easy( snd_pcm_sw_params_current( pcm_, params.get() ) );

    alsa_check_easy(
      snd_pcm_sw_params_set_avail_min( pcm_, params.get(), numeric_limits<snd_pcm_uframes_t>::max() ) );
    alsa_check_easy( snd_pcm_sw_params_set_period_event( pcm_, params.get(), false ) );
    alsa_check_easy(
      snd_pcm_sw_params_set_start_threshold( pcm_, params.get(), numeric_limits<snd_pcm_uframes_t>::max() ) );

    alsa_check_easy( snd_pcm_sw_params_set_stop_threshold(
      pcm_, params.get(), ( stream == SND_PCM_STREAM_CAPTURE ) ? 960 : ( buffer_size_ - 1 ) ) );
    alsa_check_easy( snd_pcm_sw_params_set_silence_size( pcm_, params.get(), 0 ) );
    alsa_check_easy( snd_pcm_sw_params_set_silence_threshold( pcm_, params.get(), 0 ) );

    alsa_check_easy( snd_pcm_sw_params( pcm_, params.get() ) );

    snd_pcm_uframes_t thresh;
    alsa_check_easy( snd_pcm_sw_params_get_stop_threshold( params.get(), &thresh ) );
    cerr << name() << ": stop threshold = " << thresh << "\n";
  }

  check_state( SND_PCM_STATE_PREPARED );
}

string AudioInterface::name() const
{
  return annotation_ + "[" + interface_name_ + "]";
}

AudioInterface::~AudioInterface()
{
  try {
    alsa_check( "snd_pcm_close(" + name() + ")", snd_pcm_close( pcm_ ) );
  } catch ( const exception& e ) {
    cerr << "Exception in destructor: " << e.what() << endl;
  }
}

void AudioInterface::check_state( const snd_pcm_state_t expected_state )
{
  const auto actual_state = snd_pcm_state( pcm_ );
  if ( expected_state != actual_state ) {
    throw runtime_error( name() + ": expected state " + snd_pcm_state_name( expected_state ) + " but state is "
                         + snd_pcm_state_name( actual_state ) );
  }
}

void AudioInterface::start()
{
  check_state( SND_PCM_STATE_PREPARED );
  alsa_check( "snd_pcm_start(" + name() + ")", snd_pcm_start( pcm_ ) );
  check_state( SND_PCM_STATE_RUNNING );
}

void AudioInterface::drop()
{
  alsa_check( "snd_pcm_drop(" + name() + ")", snd_pcm_drop( pcm_ ) );
  check_state( SND_PCM_STATE_SETUP );
}

void AudioInterface::debug()
{
  check_state( SND_PCM_STATE_PREPARED );
  snd_pcm_sframes_t avail, delay;
  alsa_check( "snd_pcm_avail_delay(" + name() + ")", snd_pcm_avail_delay( pcm_, &avail, &delay ) );
  cerr << name() << ": " << avail << "/" << delay << "\n";
}

void AudioInterface::loopback_to( AudioInterface& other )
{
  unsigned int loops = 0, copies = 0;

  while ( true ) {
    loops++;
    snd_pcm_sframes_t mic_avail, mic_delay;
    snd_pcm_sframes_t phone_avail, phone_delay;
    snd_pcm_avail_delay( pcm_, &mic_avail, &mic_delay );
    snd_pcm_avail_delay( other.pcm_, &phone_avail, &phone_delay );
    try {
      check_state( SND_PCM_STATE_RUNNING );
      other.check_state( SND_PCM_STATE_RUNNING );
    } catch ( const exception& e ) {
      cerr << e.what() << ": " << mic_avail << "/" << mic_delay << " " << phone_avail << "/" << phone_delay << " + "
           << copies / 48000.0 << "#" << loops << "\n";
      throw;
    }

    if ( mic_avail >= 48 and phone_avail >= 48 ) {
      cerr << "\r" << mic_avail << "/" << mic_delay << " " << phone_avail << "/" << phone_delay << " + "
           << copies / 48000.0 << "#" << loops << "             ";
      Buffer read_buf { *this, 48 };
      Buffer write_buf { other, 48 };

      for ( unsigned int i = 0; i < 48; i++ ) {
        write_buf.sample( false, i ) = read_buf.sample( false, i );
        write_buf.sample( true, i ) = read_buf.sample( true, i );
      }

      write_buf.commit();
      read_buf.commit();

      copies += 48;
      loops = 0;
    }
  }
}

void AudioInterface::link_with( AudioInterface& other )
{
  alsa_check( "snd_pcm_link(" + name() + ", " + other.name() + ")", snd_pcm_link( pcm_, other.pcm_ ) );
}

AudioInterface::Buffer::Buffer( AudioInterface& interface, const unsigned int sample_count )
  : pcm_( interface.pcm_ )
  , areas_( nullptr )
  , sample_count_( sample_count )
  , offset_( 0 )
{
  /* XXX channel count must be 2 */
  snd_pcm_uframes_t frames = sample_count;
  alsa_check_easy( snd_pcm_mmap_begin( pcm_, &areas_, &offset_, &frames ) );

  if ( frames != sample_count ) {
    throw runtime_error( interface.name() + ": could not get " + to_string( sample_count ) + " frames, got "
                         + to_string( frames ) + " instead" );
  }

  if ( areas_[0].addr != areas_[1].addr ) {
    throw runtime_error( "non-interleaved areas returned" );
  }

  if ( areas_[0].first != 0 or areas_[1].first != 32 or areas_[0].step != 64 or areas_[1].step != 64 ) {
    throw runtime_error( "unexpected format or stride returned" );
  }
}

void AudioInterface::Buffer::commit()
{
  if ( areas_ ) {
    if ( int( sample_count_ ) != alsa_check_easy( snd_pcm_mmap_commit( pcm_, offset_, sample_count_ ) ) ) {
      throw runtime_error( "short commit" );
    }

    areas_ = nullptr;
  }
}

AudioInterface::Buffer::~Buffer()
{
  if ( areas_ ) {
    cerr << "Warning, AudioInterface Buffer not committed before destruction.\n";

    try {
      commit();
    } catch ( const exception& e ) {
      cerr << "AudioInterface::Buffer destructor: " << e.what() << "\n";
    }
  }
}

void AudioInterface::write_silence( const unsigned int sample_count )
{
  Buffer buffer { *this, sample_count };

  for ( unsigned int i = 0; i < sample_count; i++ ) {
    buffer.sample( false, i ) = 0;
    buffer.sample( true, i ) = 0;
  }

  buffer.commit();

  check_state( SND_PCM_STATE_PREPARED );
}
