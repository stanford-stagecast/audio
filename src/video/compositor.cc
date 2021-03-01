#include "compositor.hh"

#include <iostream>
#include <thread>

using namespace std;

void Compositor::apply( const Scene& scene, RasterRGBA& raster )
{
  /* fill with black */
  fill( raster.pixels().begin(), raster.pixels().end(), RasterRGBA::pixel { 0, 0, 0, 255 } );

  /* apply layers in order */
  for ( auto& layer : scene.layers ) {
    if ( images_.count( layer.name ) ) {
      layer.render( *images_.at( layer.name ), raster );
    }
  }
}

void Compositor::load_image( const string& name, const shared_ptr<const RasterRGBA> image )
{
  images_.insert_or_assign( name, image );
}

Layer::Layer( const std::string_view s_name,
              const int16_t s_x,
              const int16_t s_y,
              const uint16_t s_width,
              const bool s_horizontal )
  : name( s_name )
  , x( s_x )
  , y( s_y )
  , width( s_width )
  , flip_horizontal( s_horizontal )
{}

void Layer::render( const RasterRGBA& source, RasterRGBA& output ) const
{
  uint16_t height = 720 * width / 1280;
  vector<thread> threads;

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
              const float alpha = source.pel( source_x, source_y ).alpha / 255.0;
              output.pel( target_x, target_y ).red = alpha * source.pel( source_x, source_y ).red
                                                     + ( 1 - alpha ) * output.pel( target_x, target_y ).red;
              output.pel( target_x, target_y ).green = alpha * source.pel( source_x, source_y ).green
                                                       + ( 1 - alpha ) * output.pel( target_x, target_y ).green;
              output.pel( target_x, target_y ).blue = alpha * source.pel( source_x, source_y ).blue
                                                      + ( 1 - alpha ) * output.pel( target_x, target_y ).blue;
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
