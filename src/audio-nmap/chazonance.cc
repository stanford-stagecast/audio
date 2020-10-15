#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iomanip>

#include "helpers.hh"
#include "fft.hh"
#include "audio.hh"
#include "wav.hh"
#include "constants.hh"

using namespace std;

ComplexSignal time2frequency( RealSignal & samples )
{
    ComplexSignal frequency( samples.size() / 2 + 1 );    
    FFTPair fft { samples, frequency };
    fft.time2frequency( samples, frequency );
    return frequency;
}

float dB( const float x )
{
    return 20 * log10( x );
}

struct Info
{
  struct Stats {
    double rms_amplitude;
    double peak_amplitude;
    double peak_at;

    template <typename Signal>
    Stats( const Signal & samples, const double length_in_units )
      : rms_amplitude(), peak_amplitude(), peak_at()
    {
      if ( samples.empty() ) {
	throw runtime_error( "samples cannot be empty" );
      }

      float sum = 0;
      float peak = -1;
      size_t peak_location = -1;

      for ( size_t i = 0; i < samples.size(); i++ ) {
	const float val = norm( samples[ i ] );
	sum += val;
	if ( val > peak ) {
	  peak = val;
	  peak_location = i;
	}
      }

      rms_amplitude = sqrt( sum / samples.size() );
      peak_amplitude = sqrt( peak );
      peak_at = peak_location / length_in_units;
    }
  };

  Stats time_domain, frequency_domain;

  Info( RealSignal & samples )
    : time_domain( samples, float( SAMPLE_RATE ) / samples.size() ),
      frequency_domain( time2frequency( samples ), MAX_FREQUENCY )
  {
    cout << "Time-domain stats: RMS amplitude = " << dB( time_domain.rms_amplitude ) << " dB"
	 << " and peak amplitude = " << dB( time_domain.peak_amplitude ) << " dB"
	 << " @ " << time_domain.peak_at << "\n";

    cout << "Freq-domain stats: RMS amplitude = " << dB( frequency_domain.rms_amplitude ) << " dB"
	 << " and peak amplitude = " << dB( frequency_domain.peak_amplitude ) << " dB"
	 << " @ " << frequency_domain.peak_at << "\n";
  }
};

void program_body( const string & filename )
{
    WAV wav;
    wav.read_from( filename );
    wav.samples().resize(wav.samples().size()+SAMPLE_RATE*2);

    {
	SoundCard sound_card { "default", "default" };

	if ( wav.samples().size() % sound_card.period_size() ) {
	    const size_t new_size = sound_card.period_size() * (1 + (wav.samples().size() / sound_card.period_size()));
	    cerr << "Note: WAV length of " << wav.samples().size() << " is not multiple of "
		 << sound_card.period_size() << "; resizing to " << new_size << " samples.\n";
	    wav.samples().resize( new_size );
	}
    }

    RealSignal input( wav.samples().size() );

    cout << fixed << setprecision( 2 );

    /* get overall stats */
    Info original_info( wav.samples() );
    
    for ( unsigned int iter = 0;iter < 10; iter++ ) {
	/* write out */
	//wav.write_to( filename + "-generation" + to_string( iter ) + ".wav" );
	int system_ret = system("/home/lexicologist/audio/src/audio-nmap/nmap_script.sh");
	if (system_ret == -1) return;
	/* eliminate frequencies below 20 Hz */
	ComplexSignal frequency( wav.samples().size() / 2 + 1 );
	FFTPair fft( wav.samples(), frequency );
	fft.time2frequency( wav.samples(), frequency );
	for ( unsigned int i = 0; i < frequency.size(); i++ ) {
	    const float frequency_in_hertz = i * MAX_FREQUENCY / frequency.size();
	    if ( frequency_in_hertz < 20.0 ) {
		frequency[ i ] = 0;
	    }
	}
	fft.frequency2time( frequency, wav.samples() );

	/* get stats */
	Info before_transform_info( wav.samples() );

	/* apply equal gain */
	float gain = original_info.time_domain.rms_amplitude / before_transform_info.time_domain.rms_amplitude;
	cout << "Applying gain of " << gain << "\n";
	for ( auto & sample : wav.samples() ) {
	  sample *= gain;
	}
	
	/* get stats */
	Info after_transform_info( wav.samples() );
	
	/* play and record */

	SoundCard sound_card { "default", "default" };
	sound_card.start();
	sound_card.play_and_record( wav.samples(), input );
	sound_card.stop();
	wav.samples() = input;

	cout << endl;
    }
}

int main( const int argc, const char * argv[] )
{
    if ( argc < 0 ) { abort(); }

    if ( argc != 2 ) {
        cerr << "Usage: " << argv[ 0 ] << " filename\n";
        return EXIT_FAILURE;
    }

    try {
        program_body( argv[ 1 ] );
    } catch ( const exception & e ) {
        cerr << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
