#pragma once

#include "mmap.hh"
#include "raster.hh"

#include <iostream>
#include <optional>

class VideoFile
{
  ReadOnlyFile file_;
  RasterYUV420 raster420_ { 1280, 720 };
  size_t next_byte_ {};

public:
  VideoFile( const std::string& filename )
    : file_( filename )
  {}

  const RasterYUV420& raster() { return raster420_; }

  void read_raster()
  {
    static size_t rastersize = raster420_.Y().size() + raster420_.Cb().size() + raster420_.Cr().size();
    if ( next_byte_ + rastersize <= file_.length() ) {
      const std::string_view Y_view
        = static_cast<std::string_view>( file_ ).substr( next_byte_, raster420_.Y().size() );
      next_byte_ += raster420_.Y().size();

      const std::string_view Cb_view
        = static_cast<std::string_view>( file_ ).substr( next_byte_, raster420_.Y().size() );
      next_byte_ += raster420_.Cb().size();

      const std::string_view Cr_view
        = static_cast<std::string_view>( file_ ).substr( next_byte_, raster420_.Y().size() );
      next_byte_ += raster420_.Cr().size();

      raster420_.Y().assign( Y_view.begin(), Y_view.end() );
      raster420_.Cb().assign( Cb_view.begin(), Cb_view.end() );
      raster420_.Cr().assign( Cr_view.begin(), Cr_view.end() );
    }
  }
};
