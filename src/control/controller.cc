#include "controller.hh"
#include "control_messages.hh"

using namespace std;

ClientController::ClientController( shared_ptr<NetworkClient> client,
                                    shared_ptr<AudioDeviceTask> audio_device,
                                    EventLoop& loop )
  : socket_()
  , client_( client )
  , audio_device_( audio_device )
{
  socket_.set_blocking( false );
  socket_.bind( { "127.0.0.1", control_port() } );

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
      case set_cursor_lag::id:
        set_cursor_lag my_cursor_lag;
        parser.object( my_cursor_lag );
        if ( parser.error() ) {
          return;
        }
        client_->set_cursor_lag( my_cursor_lag.num_samples );
        break;

      case set_loopback_gain::id:
        set_loopback_gain my_gain;
        parser.object( my_gain );
        if ( parser.error() ) {
          return;
        }
        audio_device_->set_loopback_gain( my_gain.gain );
    }
  } );
}
