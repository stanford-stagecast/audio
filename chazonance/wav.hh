#ifndef WAV_HH
#define WAV_HH

#include <string>

#include "helpers.hh"

class WAV
{
    RealSignal samples_ {};

public:
    void read_from( const std::string & filename );
    void write_to( const std::string & filename );

    RealSignal & samples() { return samples_; }
};

#endif /* WAV_HH */
