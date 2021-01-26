#include "controller.hh"

using namespace std;

ClientController::ClientController( shared_ptr<NetworkClient> client, EventLoop& loop )
  : socket_()
  , client_( client )
{
  socket_.set_blocking( false );
  socket_.bind( { "127.0.0.1", control_port() } );

  loop.add_rule( "control", socket_, Direction::In, [&] {
    Address src { nullptr, 0 };
    Plaintext plaintext;
    plaintext.resize( socket_.recv( src, plaintext.mutable_buffer() ) );

    Parser parser { plaintext };
    set_cursor_lag instruction;
    parser.object( instruction );
    if ( parser.error() ) {
      return;
    }

    client_->set_cursor_lag( instruction.num_samples );
  } );
}
