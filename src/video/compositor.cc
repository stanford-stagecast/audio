#include "compositor.hh"

#include <iostream>
#include <thread>

using namespace std;

void Compositor::apply( Scene& scene, RasterRGBA& raster )
{
  /* fill with black */
  fill( raster.pixels().begin(), raster.pixels().end(), RasterRGBA::pixel { 0, 0, 0, 255 } );

  /* apply layers in order */
  for ( auto& layer : scene.layers ) {
    layer.render( raster );
  }
}

void Scene::load_camera_image( const string& name, const shared_ptr<RasterRGBA> image )
{
  for ( auto& layer : layers ) {
    if ( layer.name == name and layer.type == Layer::layer_type::Camera ) {
      layer.image = image;
    }
  }
}

void Layer::render( RasterRGBA& output )
{
  uint16_t height = 720 * width / 1280;
  vector<thread> threads;

  if ( ( type == Layer::layer_type::Media ) and video ) {
    video->read_raster();
    converter.convert( video->raster(), *decoded_video_frame_ );
    image = decoded_video_frame_;
  }

  constexpr unsigned int num_threads = 8;
  for ( unsigned int i = 0; i < num_threads; i++ ) {
    threads.emplace_back(
      [&]( const unsigned int iter ) {
        const uint16_t lower_limit = iter * height / num_threads;
        const uint16_t upper_limit = ( iter + 1 ) * height / num_threads;
        for ( uint16_t row = lower_limit; row < upper_limit; row++ ) {
          for ( uint16_t col = 0; col < width; col++ ) {
            const int32_t target_x = col + x;
            const int32_t target_y = row + y;

            const uint16_t source_x = col * 1280 / width;
            const uint16_t source_y = row * 720 / height;

            if ( target_x >= 0 and target_x >= 0 and target_x < 1280 and target_y < 720 and source_x < 1280
                 and source_y < 720 ) {
              const auto src = image->pel( source_x, source_y );
              auto& outputpel = output.pel( target_x, target_y );
              float alpha = src.alpha / 255.0;

              /* temporary chroma key */
              if ( src.red <= 5 and src.green >= 100 and src.blue < 80 ) {
                alpha = 0;
              }

              outputpel.red = alpha * src.red + ( 1 - alpha ) * outputpel.red;
              outputpel.green = alpha * src.green + ( 1 - alpha ) * outputpel.green;
              outputpel.blue = alpha * src.blue + ( 1 - alpha ) * outputpel.blue;
            }
          }
        }

        return;
      },
      i );
  }

  for ( auto& thread : threads ) {
    thread.join();
  }
}

string Scene::debug_summary() const
{
  string ret;
  for ( const auto& x : layers ) {
    ret += ( x.type == Layer::layer_type::Camera ) ? "Camera: " : "Media: ";
    ret += '"' + x.name + '"';
    ret += " @ (" + to_string( x.x ) + ", " + to_string( x.y ) + "), width=" + to_string( x.width ) + "\n";
  }
  return ret;
}

void Scene::insert( Layer&& layer )
{
  for ( auto it = layers.begin(); it != layers.end(); it++ ) {
    if ( layer.z > it->z ) {
      layers.insert( it, move( layer ) );
      return;
    }
  }

  layers.push_back( move( layer ) );

  cerr << "### SCENE ###\n";
  cerr << debug_summary() << "\n\n";
}

void Scene::remove( const string_view name )
{
  if ( name == "all" ) {
    layers.clear();
  } else {
    layers.remove_if( [name]( const Layer& x ) { return x.name == name; } );
  }

  cerr << "### SCENE ###\n";
  cerr << debug_summary() << "\n\n";
}
