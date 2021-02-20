/* -*-mode:c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* Copyright 2013-2018 the Alfalfa authors
                       and the Massachusetts Institute of Technology

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

      1. Redistributions of source code must retain the above copyright
         notice, this list of conditions and the following disclaimer.

      2. Redistributions in binary form must reproduce the above copyright
         notice, this list of conditions and the following disclaimer in the
         documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include "yuv4mpeg.hh"

#include <fcntl.h>
#include <sstream>
#include <utility>
#include <algorithm>

using namespace std;

YUV4MPEGHeader::YUV4MPEGHeader( const RasterYUV422& r )
  : width( r.luma_width() )
  , height( r.height() )
  , fps_numerator( 24 )
  , fps_denominator( 1 )
  , pixel_aspect_ratio_numerator( 1 )
  , pixel_aspect_ratio_denominator( 1 )
  , interlacing_mode( InterlacingMode::PROGRESSIVE )
  , color_space( ColorSpace::C422 )
{}

size_t YUV4MPEGHeader::frame_length()
{
  if ( color_space == ColorSpace::C422 ) {
    return 2 * height * width;
  } else {
    throw runtime_error( "unimplemented" );
  }
}

size_t YUV4MPEGHeader::y_plane_length()
{
  if ( color_space == ColorSpace::C422 ) {
    return height * width;
  } else {
    throw runtime_error( "unimplemented" );
  }
}

size_t YUV4MPEGHeader::uv_plane_length()
{
  if ( color_space == ColorSpace::C422 ) {
    return height * width / 2;
  } else {
    throw runtime_error( "unimplemented" );
  }
}

string YUV4MPEGHeader::to_string()
{
  stringstream ss;
  ss << "YUV4MPEG2 " << 'W' << width << ' ' << 'H' << height << ' ' << 'F' << fps_numerator << ':'
     << fps_denominator << ' ' << 'I';

  switch ( interlacing_mode ) {
    case InterlacingMode::PROGRESSIVE:
      ss << 'p';
      break;
    case InterlacingMode::TOP_FIELD_FIRST:
      ss << 't';
      break;
    case InterlacingMode::BOTTOM_FIELD_FIRST:
      ss << 'b';
      break;
    case InterlacingMode::MIXED_MODES:
      ss << 'm';
      break;
  }

  ss << ' ' << 'A' << pixel_aspect_ratio_numerator << ':' << pixel_aspect_ratio_denominator << ' ';

  switch ( color_space ) {
    case ColorSpace::C420jpeg:
      ss << "C420jpeg XYSCSS=420JPEG";
      break;
    case ColorSpace::C420paldv:
      ss << "C420paldv XYSCSS=420PALDV";
      break;
    case ColorSpace::C420:
      ss << "C420 XYSCSS=420";
      break;
    case ColorSpace::C422:
      ss << "C422 XYSCSS=422";
      break;
    case ColorSpace::C444:
      ss << "C444 XYSCSS=444";
      break;
  }

  ss << '\n';

  return ss.str();
}

static void write_all( string_view s, FileDescriptor& fd )
{
  while ( not s.empty() ) {
    s.remove_prefix( fd.write( s ) );
  }
}

void YUV4MPEGFrameWriter::write( const RasterYUV422& r, FileDescriptor& fd )
{
  fd.write( "FRAME\n"sv );
  write_all( r.Y_view(), fd );
  write_all( r.Cb_view(), fd );
  write_all( r.Cr_view(), fd );
}
