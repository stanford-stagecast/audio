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

#include <cstdio>
#include <stdexcept>

#include "raster.hh"

using namespace std;

BaseRaster::BaseRaster( const uint16_t display_width,
                        const uint16_t display_height,
                        const uint16_t width,
                        const uint16_t height,
                        const uint8_t width_ratio,   /* = 2*/
                        const uint8_t height_ratio ) /* = 2*/
  : display_width_( display_width )
  , display_height_( display_height )
  , width_( width )
  , height_( height )
  , width_ratio_( width_ratio )
  , height_ratio_( height_ratio )
{
  if ( display_width_ > width_ ) {
    throw runtime_error( "display_width is greater than width." );
  }

  if ( display_height_ > height_ ) {
    throw runtime_error( "display_height is greater than height." );
  }
}

bool BaseRaster::operator==( const BaseRaster& other ) const
{
  return ( Y_ == other.Y_ ) and ( U_ == other.U_ ) and ( V_ == other.V_ );
}

bool BaseRaster::operator!=( const BaseRaster& other ) const
{
  return not operator==( other );
}

void BaseRaster::copy_from( const BaseRaster& other )
{
  Y_.copy_from( other.Y_ );
  U_.copy_from( other.U_ );
  V_.copy_from( other.V_ );
}

RGBRaster::RGBRaster( const uint16_t display_width,
                      const uint16_t display_height,
                      const uint16_t width,
                      const uint16_t height,
                      const uint8_t width_ratio,   /* = 1 */
                      const uint8_t height_ratio ) /* = 1 */
  : BaseRaster( display_width,
                display_height,
                width,
                height,
                width_ratio,
                height_ratio )
{}
