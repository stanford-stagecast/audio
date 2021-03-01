#pragma once

#include "raster.hh"
#include "scale.hh"
#include "vsclient.hh"

#include <list>
#include <string>

struct Layer
{
  enum class type : uint8_t
  {
    Camera
  };

  std::string name;
  int16_t x, y;
  uint16_t width;
  bool flip_horizontal;

  Layer( const std::string_view s_name,
         const int16_t s_x,
         const int16_t s_y,
         const uint16_t s_width,
         const bool s_horizontal );

  void render( const RasterRGBA& source, RasterRGBA& target ) const;
};

struct Scene
{
  std::list<Layer> layers;
};

class Compositor
{
  std::unordered_map<std::string, std::shared_ptr<const RasterRGBA>> images_ {};

public:
  void load_image( const std::string& name, const std::shared_ptr<const RasterRGBA> image );
  void apply( const Scene& scene, RasterRGBA& output );
};
