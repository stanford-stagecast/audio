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
#include "crop.hh"
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

  Camera camera { 1280, 720, "/dev/"s + device, V4L2_PIX_FMT_YUYV };

  RasterYUV420 output_raster { 1280, 720 };
  H264Encoder encoder { 1280, 720, 24, "fast", "zerolatency" };

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

  unsigned int frames_fetched_ {}, frames_encoded_ {};
  loop->add_rule( "read camera frame", camera.fd(), Direction::In, [&] {
    camera.get_next_frame( output_raster );
    frames_fetched_++;
  } );

  loop->add_rule(
    "encode",
    [&] {
      encoder.encode( output_raster );
      video_source->push( encoder.nal(), Timer::timestamp_ns() );
      encoder.reset_nal();
      frames_encoded_++;
    },
    [&] { return frames_fetched_ > frames_encoded_; } );

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
