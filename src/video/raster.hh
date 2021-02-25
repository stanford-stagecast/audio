#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

class RasterYUV
{
private:
  uint16_t width_, height_;
  uint16_t chroma_width_, chroma_height_;

  std::vector<uint8_t> Y_ {}, Cb_ {}, Cr_ {};
  std::vector<uint8_t*> Y_rows_ {}, Cb_rows_ {}, Cr_rows_ {};

protected:
  RasterYUV( const uint16_t width,
             const uint16_t height,
             const uint16_t chroma_width,
             const uint16_t chroma_height )
    : width_( width )
    , height_( height )
    , chroma_width_( chroma_width )
    , chroma_height_( chroma_height )
    , Y_( width * height )
    , Cb_( chroma_width * chroma_height )
    , Cr_( chroma_width * chroma_height )
  {
    Y_rows_.reserve( height );
    Cb_rows_.reserve( chroma_height );
    Cr_rows_.reserve( chroma_height );
    for ( uint16_t y = 0; y < height; y++ ) {
      Y_rows_.push_back( Y_row( y ) );
    }

    for ( uint16_t y = 0; y < chroma_height; y++ ) {
      Cb_rows_.push_back( Cb_row( y ) );
      Cr_rows_.push_back( Cr_row( y ) );
    }
  }

public:
  uint16_t width() const { return width_; }
  uint16_t height() const { return height_; }
  uint16_t chroma_width() const { return chroma_width_; }
  uint16_t chroma_height() const { return chroma_height_; }

  std::string_view Y_view() const { return { reinterpret_cast<const char*>( Y_.data() ), Y_.size() }; }
  std::string_view Cb_view() const { return { reinterpret_cast<const char*>( Cb_.data() ), Cb_.size() }; }
  std::string_view Cr_view() const { return { reinterpret_cast<const char*>( Cr_.data() ), Cr_.size() }; }

  const std::vector<uint8_t>& Y() const { return Y_; }
  const std::vector<uint8_t>& Cb() const { return Cb_; }
  const std::vector<uint8_t>& Cr() const { return Cr_; }

  uint8_t* Y_row( const uint16_t y ) { return &Y_.at( y * width() ); }
  uint8_t* Cb_row( const uint16_t y ) { return &Cb_.at( y * chroma_width() ); }
  uint8_t* Cr_row( const uint16_t y ) { return &Cr_.at( y * chroma_width() ); }

  const uint8_t* Y_row( const uint16_t y ) const { return &Y_.at( y * width() ); }
  const uint8_t* Cb_row( const uint16_t y ) const { return &Cb_.at( y * chroma_width() ); }
  const uint8_t* Cr_row( const uint16_t y ) const { return &Cr_.at( y * chroma_width() ); }

  std::array<uint8_t**, 3> rows( const uint16_t y )
  {
    return { &Y_rows_.at( y ), &Cb_rows_.at( y ), &Cr_rows_.at( y ) };
  }

  RasterYUV( const RasterYUV& other ) = delete;
  RasterYUV& operator=( const RasterYUV& other ) = delete;
  RasterYUV( RasterYUV&& other ) = default;
  RasterYUV& operator=( RasterYUV&& other ) = default;
};

class RasterYUV422 : public RasterYUV
{
public:
  RasterYUV422( const uint16_t width, const uint16_t height )
    : RasterYUV( width, height, width / 2, height )
  {}
};

class RasterYUV420 : public RasterYUV
{
public:
  RasterYUV420( const uint16_t width, const uint16_t height )
    : RasterYUV( width, height, width / 2, height / 2 )
  {}
};
