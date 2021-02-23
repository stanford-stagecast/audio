#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "camera.hh"
#include "h264_encoder.hh"
#include "mp4writer.hh"
#include "scale.hh"
#include "timer.hh"

using namespace std;

void program_body()
{
  ios::sync_with_stdio( false );

  FileDescriptor output { CheckSystemCall( "dup STDERR_FILENO", dup( STDOUT_FILENO ) ) };

  Camera camera { 3840, 2160, "/dev/video0" };

  RasterYUV422 camera_raster { 3840, 2160 };
  RasterYUV420 output_raster { 1280, 720 };
  H264Encoder encoder { 1280, 720, 24, "fast", "zerolatency" };
  MP4Writer writer_ { 24, 1280, 720 };

  Scaler scaler;

  unsigned int frame_no = 0;
  uint64_t last_timestamp = Timer::timestamp_ns();
  uint64_t byte_count = 0;
  while ( true ) {
    scaler.setup( frame_no, frame_no, 3840 - 2 * frame_no, 2160 - 2 * frame_no );

    camera.get_next_frame( camera_raster );
    scaler.scale( camera_raster, output_raster );
    encoder.encode( output_raster );

    if ( encoder.nal().has_value() ) {
      writer_.write( encoder.nal()->NAL, encoder.nal()->pts, encoder.nal()->dts );

      byte_count += encoder.nal()->NAL.size();
    }

    //    YUV4MPEGFrameWriter::write( output_raster, output );
    if ( frame_no and ( frame_no % 24 == 0 ) ) {
      const uint64_t this_timestamp = Timer::timestamp_ns();
      const double difference = this_timestamp - last_timestamp;
      cerr << "frames per second: " << 24.0 / ( difference / BILLION ) << "\n";
      cerr << "megabits per second: " << ( 8.0 * byte_count / MILLION ) / ( difference / BILLION ) << "\n";
      last_timestamp = this_timestamp;
      byte_count = 0;
    }

    frame_no++;
  }
}

int main()
{
  //  try {
  program_body();
  /*
} catch ( const exception& e ) {
  cerr << "Exception: " << e.what() << "\n";
  return EXIT_FAILURE;
}
  */

  return EXIT_SUCCESS;
}
