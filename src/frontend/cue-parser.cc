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
  StackBuffer<0, uint8_t, 255> buf;
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

  if ( argc != 4 ) {
    cerr << "Usage: " << argv[0] << " cue_filename cue_integer cue_fractional\n";
    return EXIT_FAILURE;
  }

  ReadOnlyFile cues { argv[1] };

  const unsigned int cue_integer_desired = stoi( argv[2] ), cue_fractional_desired = stoi( argv[3] );

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

    string filename = url;
    for ( auto& ch : filename ) {
      if ( ch == '/' ) {
        ch = '_';
      } else if ( ch == ':' ) {
        ch = '_';
      }
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
    const unsigned int cue_integer = stoi( cue_number_raw.substr( 0, cue_number_raw.size() - 2 ) );
    const unsigned int cue_fractional = stoi( cue_number_raw.substr( cue_number_raw.size() - 3, 1 ) );
    cout << cue_integer << "." << cue_fractional << ": " << cue["name"] << "\n";

    const auto changes = cue["changes"];

    for ( auto it2 = changes.begin(); it2 != changes.end(); ++it2 ) {
      const auto change = *it2;

      const auto action_type = change["action"]["type"];
      const auto change_type = change["type"];

      if ( action_type == "add" and change_type == "camera" ) {
        cout << "   ADD camera " << camera_lookup.at( change["camera_id"].asInt() ) << "\n";

        View v { change["action"]["config"]["view"] };
        cout << "        @ " << v.left << " " << v.right << " " << v.top << " " << v.bottom << " " << v.z << "\n";

        Layer new_layer;
        new_layer.type = Layer::layer_type::Camera;
        new_layer.name = camera_lookup.at( change["camera_id"].asInt() );
        new_layer.x = v.left;
        new_layer.y = v.top;
        new_layer.width = v.right - v.left;
        new_layer.z = v.z;

        scene.insert( new_layer );
      } else if ( action_type == "add" and change_type == "media" ) {
        cout << "   ADD media \"" << media_lookup.at( change["media_id"].asInt() ).first << "\"\n";

        View v { change["action"]["config"]["view"] };
        cout << "        @ " << v.left << " " << v.right << " " << v.top << " " << v.bottom << " " << v.z << "\n";

        Layer new_layer;
        new_layer.type = Layer::layer_type::Media;
        new_layer.name = media_lookup.at( change["media_id"].asInt() ).second;
        new_layer.x = v.left;
        new_layer.y = v.top;
        new_layer.width = v.right - v.left;
        new_layer.z = v.z;

        scene.insert( new_layer );
      } else if ( action_type == "remove" and change_type == "camera" ) {
        cout << "   REMOVE camera " << camera_lookup.at( change["camera_id"].asInt() ) << "\n";

        scene.remove( camera_lookup.at( change["camera_id"].asInt() ) );
      } else if ( action_type == "remove" and change_type == "media" ) {
        cout << "   REMOVE media \"" << media_lookup.at( change["media_id"].asInt() ).first << "\"\n";

        scene.remove( media_lookup.at( change["media_id"].asInt() ).second );
      }
    }

    if ( cue_integer == cue_integer_desired and cue_fractional == cue_fractional_desired ) {
      cout << "#####################\n";
      cout << "      SCENE NOW      \n\n";
      cout << scene.debug_summary();
      cout << "#####################\n\n";

      {
        remove_layer instruction;
        memcpy( instruction.name.mutable_data_ptr(), "all", strlen( "all" ) );
        instruction.name.resize( strlen( "all" ) );
        send( instruction );
      }

      for ( const auto& layer : scene.layers ) {
        insert_layer instruction;
        instruction.is_media = ( layer.type == Layer::layer_type::Media );
        instruction.name.resize( layer.name.size() );
        instruction.name.mutable_buffer().copy( layer.name );
        instruction.x = layer.x;
        instruction.y = layer.y;
        instruction.width = layer.width;

        send( instruction );
      }

      break;
    }
  }

  return EXIT_SUCCESS;
}
