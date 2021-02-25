#include "av_check.hh"

#include <array>
#include <stdexcept>

extern "C"
{
#include <libavutil/avutil.h>
}

using namespace std;

int av_check( const int retval )
{
  static array<char, 256> errbuf;

  if ( retval < 0 ) {
    if ( av_strerror( retval, errbuf.data(), errbuf.size() ) < 0 ) {
      throw runtime_error( "av_strerror: error code not found" );
    }

    errbuf.back() = 0;

    throw runtime_error( "libav error: " + string( errbuf.data() ) );
  }

  return retval;
}
