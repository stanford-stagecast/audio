#include "control_messages.hh"
#include "controller.hh"

using namespace std;

ServerController::ServerController( shared_ptr<NetworkMultiServer> server, EventLoop& loop )
  : socket_()
  , server_( server )
{
  socket_.set_blocking( false );
  socket_.bind( { "127.0.0.1", server_control_port() } );

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
      case set_cursor_lag::id: {
        set_cursor_lag my_cursor_lag;
        parser.object( my_cursor_lag );
        if ( parser.error() ) {
          return;
        }
        server_->set_cursor_lag( my_cursor_lag.name,
                                 my_cursor_lag.feed,
                                 my_cursor_lag.min_samples,
                                 my_cursor_lag.target_samples,
                                 my_cursor_lag.max_samples );
      } break;

      case set_gain::id: {
        set_gain my_gain;
        parser.object( my_gain );
        if ( parser.error() ) {
          return;
        }
        server_->set_gain( my_gain.board_name, my_gain.channel_name, my_gain.gain1, my_gain.gain2 );
      } break;
    }
  } );
}
