#include "wavwriter.hh"

using namespace std;

WavWriter::WavWriter( const string& path, const int sample_rate )
  : handle_( path, SFM_WRITE, SF_FORMAT_WAV | SF_FORMAT_FLOAT, 2, sample_rate )
{
  if ( handle_.error() ) {
    throw runtime_error( path + ": " + handle_.strError() );
  }
}

void WavWriter::write( const AudioBuffer& buffer, const size_t range_end )
{
  if ( range_end < buffer.range_begin() ) {
    throw std::out_of_range( "WavWriter::write" );
  }

  size_t next_frame_to_copy = buffer.range_begin();

  while ( next_frame_to_copy < range_end ) {
    array<pair<float, float>, 4096> sample_storage;

    unsigned int num_to_copy = min( 4096lu, range_end - next_frame_to_copy );
    for ( unsigned int i = 0; i < num_to_copy; i++ ) {
      sample_storage[i] = buffer.safe_get( next_frame_to_copy + i );
    }

    if ( num_to_copy != handle_.writef( &sample_storage.at( 0 ).first, num_to_copy ) ) {
      throw runtime_error( "writef: short write" );
    }

    next_frame_to_copy += num_to_copy;
  }
}
