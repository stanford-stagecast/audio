#include "control_messages.hh"
#include "controller.hh"

using namespace std;

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

        Layer new_layer;
        new_layer.type = my_insert_layer.is_media ? Layer::layer_type::Media : Layer::layer_type::Camera;
        new_layer.name = my_insert_layer.name;
        new_layer.x = my_insert_layer.x;
        new_layer.y = my_insert_layer.y;
        new_layer.width = my_insert_layer.width;
        new_layer.z = my_insert_layer.z;
        server->insert_preview_layer( new_layer );
      } break;

      case remove_layer::id: {
        remove_layer my_remove_layer;
        parser.object( my_remove_layer );
        if ( parser.error() ) {
          parser.clear_error();
          return;
        }

        server->remove_preview_layer( my_remove_layer.name );
      } break;
    }
  } );
}
