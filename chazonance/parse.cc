#include <cstdlib>
#include <cstring>
#include <iostream>

#include "helpers.hh"

using namespace std;

void parse_file( const string & filename )
{
    DAT dat { filename };

    const double sample_rate = 15.36 * 1.0e6;

    cout.precision( 10 );

    for ( uint64_t i = 0; i < dat.IQ_sample_count(); i++ ) {
        cout << i / sample_rate << " " << dat.I( i ) << " " << dat.Q( i ) << "\n";
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
        parse_file( argv[ 1 ] );
    } catch ( const exception & e ) {
        cerr << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
