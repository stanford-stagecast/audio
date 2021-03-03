#pragma once

#include "raster.hh"
#include "scale.hh"
#include "videofile.hh"
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
  std::shared_ptr<VideoFile> video {};
  std::shared_ptr<RasterRGBA> decoded_video_frame_ = std::make_shared<RasterRGBA>( 1280, 720 );
  std::shared_ptr<RasterRGBA> image {};
  ColorspaceConverter converter { 1280, 720 };

  void render( RasterRGBA& target );
};

struct Scene
{
  std::list<Layer> layers {};

  std::string debug_summary() const;

  void insert( Layer&& layer );
  void remove( const std::string_view name );

  void load_camera_image( const std::string& name, const std::shared_ptr<RasterRGBA> image );
};

class Compositor
{
  std::unordered_map<std::string, std::shared_ptr<const RasterRGBA>> images_ {};

public:
  void apply( Scene& scene, RasterRGBA& output );
};
