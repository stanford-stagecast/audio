#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "camera.hh"
#include "yuv4mpeg.hh"

using namespace std;

void program_body()
{
  ios::sync_with_stdio( false );

  FileDescriptor output { CheckSystemCall( "dup STDERR_FILENO", dup( STDOUT_FILENO ) ) };

  Camera camera { 3840, 2160, "/dev/video0" };

  RasterYUV422 raster { 3840, 2160 };

  YUV4MPEGHeader h { raster };
  const auto header = h.to_string();
  if ( output.write( header ) != header.size() ) {
    throw runtime_error( "short write" );
  }

  while ( true ) {
    camera.get_next_frame( raster );
    YUV4MPEGFrameWriter::write( raster, output );
  }
}

int main()
{
  try {
    program_body();
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
