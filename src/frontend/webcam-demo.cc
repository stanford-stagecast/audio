#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "camera.hh"
#include "scale.hh"
#include "yuv4mpeg.hh"

using namespace std;

void program_body()
{
  ios::sync_with_stdio( false );

  FileDescriptor output { CheckSystemCall( "dup STDERR_FILENO", dup( STDOUT_FILENO ) ) };

  Camera camera { 3840, 2160, "/dev/video0" };

  RasterYUV422 camera_raster { 3840, 2160 };
  RasterYUV420 output_raster { 1280, 720 };

  YUV4MPEGHeader h { output_raster };
  const auto header = h.to_string();
  if ( output.write( header ) != header.size() ) {
    throw runtime_error( "short write" );
  }

  Scaler scaler;

  unsigned int frame_no = 0;
  while ( true ) {
    scaler.setup( frame_no, frame_no, 3840 - 2 * frame_no, 2160 - 2 * frame_no );
    camera.get_next_frame( camera_raster );
    scaler.scale( camera_raster, output_raster );
    YUV4MPEGFrameWriter::write( output_raster, output );
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
