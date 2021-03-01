#include "compositor.hh"

using namespace std;

void Compositor::black( RasterYUV420& raster )
{
  fill( raster.Y().begin(), raster.Y().end(), 16 );
  fill( raster.Cb().begin(), raster.Cb().end(), 128 );
  fill( raster.Cr().begin(), raster.Cr().end(), 128 );
}
