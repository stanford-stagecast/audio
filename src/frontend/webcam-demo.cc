#include "camera.hh"
#include "eventloop.hh"
#include "h264_encoder.hh"
#include "mp4writer.hh"
#include "scale.hh"

#include <cstdlib>
#include <unistd.h>

using namespace std;

int main()
{
  Camera camera { 3840, 2160, "/dev/video0" };
  Scaler scaler;
  scaler.setup( 0, 0, 3840, 2160 );
  H264Encoder encoder { 1280, 720, 24, "veryfast", "zerolatency" };
  MP4Writer muxer { 24, 1280, 720 };

  RasterYUV422 camera_frame { 3840, 2160 };
  RasterYUV420 scaled_frame { 1280, 720 };

  FileDescriptor output { CheckSystemCall( "dup", dup( STDOUT_FILENO ) ) };

  EventLoop loop;

  loop.add_rule( "encode frame", camera.fd(), Direction::In, [&] {
    camera.get_next_frame( camera_frame );
    scaler.scale( camera_frame, scaled_frame );
    encoder.encode( scaled_frame );
    if ( encoder.has_nal() ) {
      muxer.write( encoder.nal().NAL, encoder.frames_encoded() - 1, encoder.frames_encoded() - 1 );
      encoder.reset_nal();
    }
    if ( muxer.output().readable_region().size() > 0 ) {
      muxer.output().pop_to_fd( output );
    }
  } );

  while ( loop.wait_next_event( -1 ) != EventLoop::Result::Exit ) {
  }

  return EXIT_SUCCESS;
}
