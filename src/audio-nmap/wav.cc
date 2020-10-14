#include <iostream>
#include <exception>
#include <iomanip>

#include <sndfile.hh>

#include "wav.hh"
#include "constants.hh"

using namespace std;

void check( const string & info, const SndfileHandle & handle )
{
    if ( handle.error() ) {
	throw runtime_error( info + ": " + handle.strError() );
    }

    if ( handle.format() != (SF_FORMAT_WAV | SF_FORMAT_FLOAT) ) {
	throw runtime_error( info + ": not a 32-bit float PCM WAV file" );
    }

    if ( handle.samplerate() != SAMPLE_RATE ) {
	throw runtime_error( info + " sample rate is " + to_string( handle.samplerate() ) + ", not " + to_string( SAMPLE_RATE ) );
    }

    if ( handle.channels() != NUM_CHANNELS ) {
	throw runtime_error( info + " channel # is " + to_string( handle.channels() ) + ", not " + to_string( NUM_CHANNELS ) );
    }

}

void WAV::read_from( const string & filename )
{
    SndfileHandle handle_{ filename };

    check( filename, handle_ );

    cerr << fixed << setprecision( 2 ) << "File " << filename << " duration: "
	 << float( handle_.frames() ) / SAMPLE_RATE << " s.\n";

    samples_.resize( handle_.frames() );

    /* read file into memory */
    const auto retval = handle_.read( samples_.data(), samples_.size() );
    if ( retval != int( samples_.size() ) ) {
	throw runtime_error( "unexpected read of " + to_string( retval ) + " samples" );
    }

    /* verify EOF */
    float dummy;
    if ( 0 != handle_.read( &dummy, 1 ) ) {
	throw runtime_error( "unexpected extra data in WAV file" );
    }
}

void WAV::write_to( const string & filename )
{
    SndfileHandle handle_{ filename,
			   SFM_WRITE,
			   SF_FORMAT_WAV | SF_FORMAT_FLOAT,
			   NUM_CHANNELS,
			   SAMPLE_RATE };

    check( "writing to " + filename, handle_ );

    cerr << fixed << setprecision( 2 ) << "Writing to file " << filename << " (duration: "
	 << float( samples_.size() ) / SAMPLE_RATE << " s).\n";

    /* write file */
    const auto retval = handle_.write( samples_.data(), samples_.size() );
    if ( retval != int( samples_.size() ) ) {
	throw runtime_error( "unexpected write of " + to_string( retval ) + " samples" );
    }
}
