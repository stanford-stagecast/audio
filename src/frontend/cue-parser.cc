#include "compositor.hh"
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

struct View
{
  uint32_t top, right, bottom, left, z;

  View( const Json::Value& node )
    : top( node["top"].asUInt() )
    , right( node["right"].asUInt() )
    , bottom( node["bottom"].asUInt() )
    , left( node["left"].asUInt() )
    , z( node["z"].asUInt() )
  {}
};

template<class Message>
void send( const Message& message )
{
  StackBuffer<0, uint16_t, 65535> buf;
  Serializer s { buf.mutable_buffer() };
  s.integer( Message::id );
  s.object( message );
  buf.resize( s.bytes_written() );

  UDPSocket socket;
  socket.sendto( { "127.0.0.1", video_server_control_port() }, buf );
}

int main( int argc, char* argv[] )
{
  if ( argc <= 0 ) {
    abort();
  }

  if ( argc != 3 ) {
    cerr << "Usage: " << argv[0] << " cue_filename desired_cue\n";
    return EXIT_FAILURE;
  }

  ReadOnlyFile cues { argv[1] };

  const string desired_cue = argv[2];

  istringstream istr;
  istr.str( string( cues ) );

  Json::Value root;
  istr >> root;

  /* learn camera mapping */
  unordered_map<int, string> camera_lookup;
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

    camera_lookup.insert_or_assign( id, name );
  }

  /* learn media */
  unordered_map<int, pair<string, string>> media_lookup;
  for ( auto it = root["media"].begin(); it != root["media"].end(); it++ ) {
    const auto node = *it;
    const int id = node["id"].asInt();
    const string url = node["file"].asString();
    const string name = node["name"].asString();
    const string type = node["type"].asString();

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

    filename += ".rawvideo";

    media_lookup.insert_or_assign( id, make_pair( name, filename ) );
  }

  Scene scene;

  /* go through cues in order */
  for ( auto it = root["cues"].begin(); it != root["cues"].end(); it++ ) {
    const auto cue = *it;
    const string cue_number_raw = cue["number"].asString();
    if ( cue_number_raw.size() < 2 ) {
      throw runtime_error( "invalid cue number: " + cue_number_raw );
    }
    //    cout << cue_number_raw << cue["name"] << "\n";

    const auto changes = cue["changes"];

    for ( auto it2 = changes.begin(); it2 != changes.end(); ++it2 ) {
      const auto change = *it2;

      const auto action_type = change["action"]["type"];
      const auto change_type = change["type"];

      if ( action_type == "add" and change_type == "camera" ) {
        //        cout << "   ADD camera " << camera_lookup.at( change["camera_id"].asInt() ) << "\n";

        View v { change["action"]["config"]["view"] };
        //        cout << "        @ " << v.left << " " << v.right << " " << v.top << " " << v.bottom << " " << v.z
        //        << "\n";

        Layer new_layer;
        new_layer.type = Layer::layer_type::Camera;
        new_layer.name = camera_lookup.at( change["camera_id"].asInt() );
        new_layer.x = v.left;
        new_layer.y = v.top;
        new_layer.width = v.right - v.left;
        new_layer.z = v.z;

        scene.insert( move( new_layer ) );
      } else if ( action_type == "add" and change_type == "media" ) {
        //        cout << "   ADD media \"" << media_lookup.at( change["media_id"].asInt() ).first << "\"\n";

        View v { change["action"]["config"]["view"] };
        //        cout << "        @ " << v.left << " " << v.right << " " << v.top << " " << v.bottom << " " << v.z
        //        << "\n";

        Layer new_layer;
        new_layer.type = Layer::layer_type::Media;
        new_layer.name = media_lookup.at( change["media_id"].asInt() ).first;
        new_layer.filename = media_lookup.at( change["media_id"].asInt() ).second;
        new_layer.x = v.left;
        new_layer.y = v.top;
        new_layer.width = v.right - v.left;
        new_layer.z = v.z;

        scene.insert( move( new_layer ) );
      } else if ( action_type == "remove" and change_type == "camera" ) {
        //        cout << "   REMOVE camera " << camera_lookup.at( change["camera_id"].asInt() ) << "\n";

        scene.remove( camera_lookup.at( change["camera_id"].asInt() ) );
      } else if ( action_type == "remove" and change_type == "media" ) {
        //        cout << "   REMOVE media \"" << media_lookup.at( change["media_id"].asInt() ).first << "\"\n";

        scene.remove( media_lookup.at( change["media_id"].asInt() ).first );
      }
    }

    if ( desired_cue == cue_number_raw ) {
      cout << "#####################\n";
      cout << "      SCENE NOW      \n\n";
      cout << "Cue: " << cue_number_raw << "\n";
      cout << scene.debug_summary();
      cout << "#####################\n\n";

      atomic_scene_update inst;

      {
        remove_layer removal;
        memcpy( removal.name.mutable_data_ptr(), "all", strlen( "all" ) );
        removal.name.resize( strlen( "all" ) );
        inst.removals.push_back( removal );
      }

      for ( const auto& layer : scene.layers ) {
        insert_layer insertion;
        insertion.is_media = ( layer.type == Layer::layer_type::Media );
        insertion.name.resize( layer.name.size() );
        insertion.name.mutable_buffer().copy( layer.name );
        insertion.filename.resize( layer.filename.size() );
        insertion.filename.mutable_buffer().copy( layer.filename );
        insertion.x = layer.x;
        insertion.y = layer.y;
        insertion.width = min( layer.width, uint16_t( 1280 ) );
        inst.insertions.push_back( insertion );
      }

      send( inst );

      break;
    }
  }

  return EXIT_SUCCESS;
}
