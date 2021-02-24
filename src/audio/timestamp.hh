#pragma once

#include <iomanip>
#include <ostream>

inline void pp_samples( std::ostream& out, const int64_t sample_count )
{
  int64_t s = sample_count;
  bool negative = false;

  if ( s < 0 ) {
    out << "-{";
    negative = true;
    s = -s;
  }

  const size_t minutes = s / ( 48000 * 60 );
  s = s % ( 48000 * 60 );

  const size_t seconds = s / 48000;
  s = s % 48000;

  const size_t ms = s / 48;
  s = s % 48;

  if ( minutes ) {
    out << std::setw( 2 ) << std::setfill( '0' ) << minutes << "m";
  }

  out << std::setw( 2 ) << std::setfill( '0' ) << seconds << "s";
  out << std::setw( 3 ) << std::setfill( '0' ) << ms;
  if ( s ) {
    out << "+" << std::setw( 2 ) << std::setfill( '0' ) << s;
  } else {
    out << "   ";
  }

  if ( negative ) {
    out << "}";
  }
}
