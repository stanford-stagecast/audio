#pragma once

#include "audio_buffer.hh"

#include <sndfile.hh>
#include <string>

class WavWriter
{
  SndfileHandle handle_;

public:
  WavWriter( const std::string& path, const int sample_rate );

  void write( const AudioBuffer& buffer, const size_t range_end );
};
