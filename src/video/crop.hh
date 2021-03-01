#pragma once

#include "raster.hh"

#include <cstdint>
#include <cstring>

class Cropper
{
  uint16_t left_ {}, right_ {}, top_ {}, bottom_ {};

public:
  void setup( const uint16_t left, const uint16_t right, const uint16_t top, const uint16_t bottom )
  {
    left_ = left;
    right_ = right;
    top_ = top;
    bottom_ = bottom;
  }

  void crop( RasterYUV422& raster )
  {
    for ( uint16_t row = 0; row < raster.height(); row++ ) {
      if ( row < top_ ) {
        memset( raster.Y_row( row ), 111, raster.width() );
        memset( raster.Cb_row( row ), 101, raster.chroma_width() );
        memset( raster.Cr_row( row ), 48, raster.chroma_width() );
      } else if ( row < raster.height() - std::min( bottom_, raster.height() ) ) {
        memset( raster.Y_row( row ), 111, raster.width() );
        memset( raster.Cb_row( row ), 101, raster.chroma_width() );
        memset( raster.Cr_row( row ), 48, raster.chroma_width() );
      } else {
        memset( raster.Y_row( row ), 111, std::min( raster.width(), left_ ) );
        memset( raster.Cr_row( row ), 101, std::min( raster.chroma_width(), uint16_t( left_ / 2 ) ) );
        memset( raster.Cr_row( row ), 48, std::min( raster.chroma_width(), uint16_t( left_ / 2 ) ) );

        const uint16_t right_margin = std::min( raster.width(), right_ );
        const uint16_t right_margin_chroma = std::min( raster.chroma_width(), uint16_t( right_ / 2 ) );
        memset( raster.Y_row( row ) + raster.width() - right_margin, 111, right_margin );
        memset( raster.Cb_row( row ) + raster.chroma_width() - right_margin_chroma, 101, right_margin_chroma );
        memset( raster.Cr_row( row ) + raster.chroma_width() - right_margin_chroma, 48, right_margin_chroma );
      }
    }
  }
};
