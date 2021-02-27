#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "address.hh"
#include "camera.hh"
#include "connection.hh"
#include "crypto.hh"
#include "eventloop.hh"
#include "exception.hh"
#include "h264_encoder.hh"
#include "keys.hh"
#include "scale.hh"
#include "socket.hh"
#include "stats_printer.hh"
#include "timer.hh"
#include "video_source.hh"
#include "videoclient.hh"

using namespace std;

void program_body( const string& device, const string& host, const string& service, const string& key_filename )
{
  ios::sync_with_stdio( false );

  FileDescriptor output { CheckSystemCall( "dup STDERR_FILENO", dup( STDOUT_FILENO ) ) };

  Camera camera { 3840, 2160, "/dev/"s + device };

  RasterYUV422 camera_raster { 3840, 2160 };
  RasterYUV420 output_raster { 1280, 720 };
  H264Encoder encoder { 1280, 720, 24, "fast", "zerolatency" };
  Scaler scaler;

  /* read key */
  ReadOnlyFile keyfile { key_filename };
  Parser p { keyfile };
  LongLivedKey key { p };

  cerr << "Starting client as " << key.name() << ".\n";

  auto loop = make_shared<EventLoop>();

  const Address stagecast_server { host, service };
  UDPSocket socket {};

  CryptoSession long_lived_crypto { key.key_pair().uplink, key.key_pair().downlink };

  auto video_source = make_shared<VideoSource>();

  auto client = make_shared<VideoClient>( stagecast_server, key, video_source, *loop );

  unsigned int frames_fetched_ {}, frames_scaled_ {}, frames_encoded_ {};
  loop->add_rule( "read camera frame", camera.fd(), Direction::In, [&] {
    camera.get_next_frame( camera_raster );
    frames_fetched_++;
  } );

  loop->add_rule(
    "scale frame",
    [&] {
      if ( client->has_control() ) {
        scaler.setup( client->control().x, client->control().y, client->control().width, client->control().height );
        client->pop_control();
      }

      scaler.scale( camera_raster, output_raster );
      frames_scaled_++;
    },
    [&] { return frames_fetched_ > frames_scaled_; } );

  loop->add_rule(
    "encode",
    [&] {
      encoder.encode( output_raster );
      video_source->push( encoder.nal(), Timer::timestamp_ns() );
      encoder.reset_nal();
      frames_encoded_++;
    },
    [&] { return frames_scaled_ > frames_encoded_; } );

  /* Print out statistics to terminal */
  StatsPrinterTask stats_printer { loop };
  stats_printer.add( client );
  stats_printer.add( video_source );

  while ( loop->wait_next_event( video_source->wait_time_ms( Timer::timestamp_ns() ) )
          != EventLoop::Result::Exit ) {
  }
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }

    if ( argc != 5 ) {
      cerr << "Usage: " << argv[0] << " device [e.g. video0] host service keyfile\n";
      return EXIT_FAILURE;
    }

    program_body( argv[1], argv[2], argv[3], argv[4] );
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << "\n";
    global_timer().summary( cerr );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
