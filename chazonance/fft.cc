#include "fft.hh"

#include <iostream>
#include <cstring>
#include <thread>
#include <type_traits>

using namespace std;

/* make sure that global FFTW state is cleaned up when program exits */
class FFTW
{
public:
    FFTW()
    {
        if ( not fftwf_init_threads() ) {
            throw unix_error( "fftwf_init_threads" );
        }

        if ( thread::hardware_concurrency() > 1 ) {
            fftwf_plan_with_nthreads( thread::hardware_concurrency() / 2 );
        }

	static_assert( sizeof(fftwf_complex) == sizeof(complex<float>),
		       "fftwf_complex must be same as complex<float>" );
    }

    ~FFTW() { fftwf_cleanup_threads(); }
};

FFTW global_fftw_state;

FFTPlan::FFTPlan( const string & what, const fftwf_plan p ) {
    if ( p == nullptr ) { throw runtime_error( what ); }
    plan = p;
}

FFTPlan::~FFTPlan() {
    if ( plan ) { fftwf_destroy_plan( plan ); }
}

FFTPlan::FFTPlan( FFTPlan && other )
    : plan( other.plan )
{
    if ( this != &other ) {
	other.plan = nullptr;
    }
}

FFTPlan & FFTPlan::operator=( FFTPlan && other )
{
    if ( this != &other ) {
	plan = other.plan;
	other.plan = nullptr;
    }
    return *this;
}

/* helper conversions for FFTW */
float * raw( RealSignal & signal ) {
    return signal.data();
}

fftwf_complex * raw( ComplexSignal & signal ) {
    return reinterpret_cast<fftwf_complex *>( signal.data() );
}

void FFTPair::check( RealSignal & time, ComplexSignal & frequency ) const
{
    if ( time.size() != N ) {
	throw runtime_error( "length of time-domain signal must be N (" + to_string(N) + ")" );
    }

    if ( frequency.size() != N/2 + 1 ) {
	throw runtime_error( "length of frequency-domain signal must be N/2 + 1 (with N = " + to_string(N) + ")" );
    }

    if ( fftwf_alignment_of( raw( time ) ) ) {
        throw runtime_error( "time-domain signal not sufficiently aligned" );
    }

    if ( fftwf_alignment_of( reinterpret_cast<float *>( raw( frequency ) ) ) ) {
	throw runtime_error( "frequency-domain signal not sufficiently aligned" );
    }
}

FFTPair::FFTPair( RealSignal & time, ComplexSignal & frequency )
    : N( time.size() ),
      forward(), backward()
{
    if ( N < 1 ) {
	throw runtime_error( "time-domain signal must have at least one sample" );
    }

    check( time, frequency );

    forward = FFTPlan( "fftwf_plan_dft_r2c_1d",
		       fftwf_plan_dft_r2c_1d( N,
					      raw( time ),
					      raw( frequency ),
					      FFTW_ESTIMATE ) );

    backward = FFTPlan( "fftwf_plan_dft_c2r_1d",
			fftwf_plan_dft_c2r_1d( N,
					       raw( frequency ),
					       raw( time ),
					       FFTW_ESTIMATE ) );
}

void FFTPair::time2frequency( RealSignal & time, ComplexSignal & frequency )
{
    check( time, frequency );

    fftwf_execute_dft_r2c( forward, raw( time ), raw( frequency ) );

    for ( auto & x : frequency ) { x /= sqrt( N ); }
}

void FFTPair::frequency2time( ComplexSignal & frequency, RealSignal & time )
{
    check( time, frequency );

    fftwf_execute_dft_c2r( backward, raw( frequency ), raw( time) );

    for ( auto & x : time ) { x /= sqrt( N ); }
}
