#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

class RasterYUV422
{
private:
  uint16_t width_, height_;
  std::vector<uint8_t> Y_, Cb_, Cr_;
  std::vector<uint8_t*> Y_rows_, Cb_rows_, Cr_rows_;

public:
  RasterYUV422( const uint16_t width, const uint16_t height )
    : width_( width )
    , height_( height )
    , Y_( width * height )
    , Cb_( width * height / 2 )
    , Cr_( width * height / 2 )
    , Y_rows_()
    , Cb_rows_()
    , Cr_rows_()
  {
    if ( width % 2 or height % 2 ) {
      throw std::runtime_error( "invalid dimension" );
    }

    Y_rows_.reserve( height );
    Cb_rows_.reserve( height );
    Cr_rows_.reserve( height );
    for ( uint16_t y = 0; y < height; y++ ) {
      Y_rows_.push_back( Y_row( y ) );
      Cb_rows_.push_back( Cb_row( y ) );
      Cr_rows_.push_back( Cr_row( y ) );
    }
  }

  uint16_t height() const { return height_; }
  uint16_t luma_width() const { return width_; }
  uint16_t chroma_width() const { return width_ / 2; }

  std::string_view Y_view() const { return { reinterpret_cast<const char*>( Y_.data() ), Y_.size() }; }
  std::string_view Cb_view() const { return { reinterpret_cast<const char*>( Cb_.data() ), Cb_.size() }; }
  std::string_view Cr_view() const { return { reinterpret_cast<const char*>( Cr_.data() ), Cr_.size() }; }

  const std::vector<uint8_t>& Y() const { return Y_; }
  const std::vector<uint8_t>& Cb() const { return Cb_; }
  const std::vector<uint8_t>& Cr() const { return Cr_; }

  uint8_t* Y_row( const uint16_t y ) { return &Y_.at( y * luma_width() ); }
  uint8_t* Cb_row( const uint16_t y ) { return &Cb_.at( y * chroma_width() ); }
  uint8_t* Cr_row( const uint16_t y ) { return &Cr_.at( y * chroma_width() ); }

  std::array<uint8_t**, 3> rows( const uint16_t y )
  {
    return { &Y_rows_.at( y ), &Cb_rows_.at( y ), &Cr_rows_.at( y ) };
  }

  RasterYUV422( const RasterYUV422& other ) = delete;
  RasterYUV422& operator=( const RasterYUV422& other ) = delete;
  RasterYUV422( RasterYUV422&& other ) = delete;
  RasterYUV422& operator=( RasterYUV422&& other ) = delete;
};
