#include "control_messages.hh"
#include "controller.hh"

using namespace std;

Layer make_layer( const insert_layer& instruction )
{
  Layer new_layer;
  new_layer.type = instruction.is_media ? Layer::layer_type::Media : Layer::layer_type::Camera;
  new_layer.name = instruction.name;
  new_layer.filename = instruction.filename;
  new_layer.x = instruction.x;
  new_layer.y = instruction.y;
  new_layer.width = min( instruction.width, uint16_t( 1280 ) );
  new_layer.z = instruction.z;
  return new_layer;
}

VideoServerController::VideoServerController( shared_ptr<VideoServer> server, EventLoop& loop )
  : socket_()
  , server_( server )
{
  socket_.set_blocking( false );
  socket_.bind( { "127.0.0.1", video_server_control_port() } );

  loop.add_rule( "control", socket_, Direction::In, [&] {
    Address src { nullptr, 0 };
    Plaintext plaintext;
    plaintext.resize( socket_.recv( src, plaintext.mutable_buffer() ) );

    Parser parser { plaintext };
    uint8_t control_id;
    parser.integer( control_id );
    if ( parser.error() ) {
      return;
    }

    switch ( control_id ) {
      case set_live::id: {
        set_live my_set_live;
        parser.object( my_set_live );
        if ( parser.error() ) {
          parser.clear_error();
          return;
        }
        server_->set_live( my_set_live.name );
      } break;
      case video_control::id: {
        video_control my_video_control;
        parser.object( my_video_control );
        if ( parser.error() ) {
          parser.clear_error();
          return;
        }
        server_->set_zoom( my_video_control );
      } break;
      case insert_layer::id: {
        insert_layer my_insert_layer;
        parser.object( my_insert_layer );
        if ( parser.error() ) {
          parser.clear_error();
          return;
        }

        server_->insert_preview_layer( make_layer( my_insert_layer ) );
      } break;

      case remove_layer::id: {
        remove_layer my_remove_layer;
        parser.object( my_remove_layer );
        if ( parser.error() ) {
          parser.clear_error();
          return;
        }

        server_->remove_preview_layer( my_remove_layer.name );
      } break;

      case atomic_scene_update::id: {
        atomic_scene_update my_update;
        parser.object( my_update );
        if ( parser.error() ) {
          parser.clear_error();
          return;
        }

        for ( const auto& x : my_update.removals ) {
          server_->remove_preview_layer( x.name );
        }

        for ( const auto& x : my_update.insertions ) {
          server_->insert_preview_layer( make_layer( x ) );
        }

      }; break;
    }
  } );
}
