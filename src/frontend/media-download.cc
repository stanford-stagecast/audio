#include "exception.hh"
#include "mmap.hh"

#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <json/json.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unordered_map>

using namespace std;

int main( int argc, char* argv[] )
{
  if ( argc <= 0 ) {
    abort();
  }

  if ( argc != 2 ) {
    cerr << "Usage: " << argv[0] << " cue_filename\n";
    return EXIT_FAILURE;
  }

  ReadOnlyFile cues { argv[1] };

  istringstream istr;
  istr.str( string( cues ) );

  Json::Value root;
  istr >> root;

  /* learn camera mapping */
  unordered_map<int, string> camera_id_to_name;
  for ( auto it = root["cameras"].begin(); it != root["cameras"].end(); it++ ) {
    const auto node = *it;
    const int id = node["id"].asInt();
    string name = node["name"].asString();
    if ( name == "Rau" ) {
      name = "Michael";
    } else if ( name == "Gelsey Vocals" ) {
      name = "Gelsey";
    } else if ( name == "Mariel Cello" ) {
      name = "Mariel";
    } else if ( name == "Josh Violin" ) {
      name = "Josh";
    }

    camera_id_to_name.insert_or_assign( id, name );
  }

  /* list media */
  for ( auto it = root["media"].begin(); it != root["media"].end(); it++ ) {
    const auto node = *it;
    //    const int id = node["id"].asInt();
    const string url = node["file"].asString();
    const string type = node["type"].asString();
    const string name = node["name"].asString();

    string filename = name;
    for ( auto& ch : filename ) {
      if ( ch == '/' ) {
        ch = '_';
      } else if ( ch == ':' ) {
        ch = '_';
      } else if ( ch == ' ' ) {
        ch = '_';
      }
    }

    if ( type == "image" ) {
      filename += ".png";
    } else if ( type == "audio" ) {
      filename += ".wav";
    } else if ( type == "video" ) {
      filename += ".mp4";
    }

    cout << "wget -N -O '" << filename << "' '" << url << "'\n";
  }

  return EXIT_SUCCESS;
}
