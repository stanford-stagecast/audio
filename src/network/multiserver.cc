#include "multiserver.hh"

#include <iostream>

using namespace std;

NetworkMultiServer::NetworkMultiServer( EventLoop& )
  : socket_()
{
  socket_.set_blocking( false );
  socket_.bind( { "0", 0 } );

  cout << "Port " << socket_.local_address().port() << " " << receive_key_.printable_key().as_string_view() << " "
       << send_key_.printable_key().as_string_view() << endl;
}
