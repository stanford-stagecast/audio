#pragma once

#include "raster.hh"
#include "scale.hh"
#include "vsclient.hh"

#include <list>
#include <string>

struct Layer
{
  enum class layer_type : uint8_t
  {
    Camera,
    Media
  };

  layer_type type {};
  std::string name {};
  std::string filename {};
  int16_t x {}, y {};
  uint16_t width {};
  uint16_t z {};

  void render( const RasterRGBA& source, RasterRGBA& target ) const;
};

struct Scene
{
  std::list<Layer> layers {};

  std::string debug_summary() const;

  void insert( const Layer& layer );
  void remove( const std::string_view name );
};

class Compositor
{
  std::unordered_map<std::string, std::shared_ptr<const RasterRGBA>> images_ {};

public:
  void load_image( const std::string& name, const std::shared_ptr<const RasterRGBA> image );
  void apply( const Scene& scene, RasterRGBA& output );
};
