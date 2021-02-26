#include "camera.hh"
#include "eventloop.hh"
#include "h264_encoder.hh"
#include "mp4writer.hh"
#include "scale.hh"
#include "ws_server.hh"

#include <csignal>
#include <cstdlib>
#include <unistd.h>

using namespace std;

struct EventCategories
{
  size_t close, tcp_read, tcp_write, ws_handshake, ws_receive, ws_send;

  EventCategories( EventLoop& loop )
    : close( loop.add_category( "close" ) )
    , tcp_read( loop.add_category( "read" ) )
    , tcp_write( loop.add_category( "write" ) )
    , ws_handshake( loop.add_category( "WebSocket handshake" ) )
    , ws_receive( loop.add_category( "WebSocket receive" ) )
    , ws_send( loop.add_category( "WebSocket send" ) )
  {}
};

class ClientConnection
{
  TCPSocket socket_;
  WebSocketServer ws_server_ { "file://" };
  MP4Writer muxer_ { 24, 1280, 720 };
  WebSocketFrame ws_frame_ {};

  RingBuffer inbound_ { 1048576 }, outbound_ { 1048576 };

  unsigned int video_frame_count_ {};

  vector<EventLoop::RuleHandle> rules_ {};

  bool good_ = true;
  shared_ptr<bool> cull_needed_;

  void cull()
  {
    *cull_needed_ = true;
    good_ = false;

    for ( auto& rule : rules_ ) {
      rule.cancel();
    }

    rules_.clear();
  }

public:
  const TCPSocket& socket() { return socket_; }

  bool good() const { return good_; }

  void push_NAL( const string_view s )
  {
    muxer_.write( s, video_frame_count_, video_frame_count_ );
    video_frame_count_++;
  }

  ClientConnection( const EventCategories& categories,
                    TCPSocket& listening_socket,
                    EventLoop& loop,
                    shared_ptr<bool> cull_needed )
    : socket_( [&] {
      auto sock = listening_socket.accept();
      sock.set_blocking( false );
      sock.set_tcp_nodelay( true );
      return sock;
    }() )
    , cull_needed_( cull_needed )
  {
    cerr << "New connection from " << socket_.peer_address().to_string() << "\n";

    rules_.reserve( 10 );

    rules_.push_back( loop.add_rule(
      categories.close, [this] { cull(); }, [this] { return good() and ws_server_.should_close_connection(); } ) );

    rules_.push_back( loop.add_rule(
      categories.tcp_read,
      socket_,
      Direction::In,
      [this] { inbound_.push_from_fd( socket_ ); },
      [this] { return good() and inbound_.writable_region().size(); },
      [this] { cull(); } ) );

    rules_.push_back( loop.add_rule(
      categories.tcp_write,
      socket_,
      Direction::Out,
      [this] { outbound_.pop_to_fd( socket_ ); },
      [this] { return good() and outbound_.readable_region().size(); },
      [this] { cull(); } ) );

    rules_.push_back( loop.add_rule(
      categories.ws_handshake,
      [this] { ws_server_.do_handshake( inbound_, outbound_ ); },
      [this] {
        return good() and ( not inbound_.readable_region().empty() ) and ( not ws_server_.handshake_complete() );
      } ) );

    ws_frame_.fin = true;
    ws_frame_.opcode = WebSocketFrame::opcode_t::Binary;

    rules_.push_back( loop.add_rule(
      categories.ws_send,
      [this] {
        ws_frame_.payload.resize( min( muxer_.output().readable_region().size(),
                                       outbound_.writable_region().size() - WebSocketFrame::max_overhead() ) );
        memcpy( ws_frame_.payload.data(), muxer_.output().readable_region().data(), ws_frame_.payload.size() );

        muxer_.output().pop( ws_frame_.payload.size() );

        Serializer s { outbound_.writable_region() };
        s.object( ws_frame_ );
        outbound_.push( s.bytes_written() );
      },
      [&] {
        return outbound_.writable_region().size() > WebSocketFrame::max_overhead()
               and muxer_.output().readable_region().size() and ws_server_.handshake_complete();
      } ) );

    rules_.push_back( loop.add_rule(
      categories.ws_receive,
      [this] {
        ws_server_.endpoint().read( inbound_, outbound_ );
        if ( ws_server_.endpoint().ready() ) {
          cerr << "got WS message\n";
          ws_server_.endpoint().pop_message();
        }
      },
      [this] { return good() and inbound_.readable_region().size(); } ) );
  }
};

int main()
{
  ios::sync_with_stdio( false );

  if ( SIG_ERR == signal( SIGPIPE, SIG_IGN ) ) {
    throw unix_error( "signal" );
  }

  Camera camera { 3840, 2160, "/dev/video0" };
  RasterYUV422 camera_frame { 3840, 2160 };
  RasterYUV420 scaled_frame { 1280, 720 };
  Scaler scaler;
  scaler.setup( 0, 0, 3840, 2160 );
  H264Encoder encoder { 1280, 720, 24, "veryfast", "zerolatency" };

  TCPSocket web_listen_socket;
  web_listen_socket.set_reuseaddr();
  web_listen_socket.set_blocking( false );
  web_listen_socket.set_tcp_nodelay( true );
  web_listen_socket.bind( { "127.0.0.1", 8400 } );
  web_listen_socket.listen();

  list<ClientConnection> clients {};

  EventLoop loop;
  EventCategories categories { loop };

  loop.add_rule( "encode frame", camera.fd(), Direction::In, [&] {
    camera.get_next_frame( camera_frame );
    scaler.scale( camera_frame, scaled_frame );
    encoder.encode( scaled_frame );
    if ( encoder.has_nal() ) {
      for ( ClientConnection& client : clients ) {
        client.push_NAL( encoder.nal().NAL );
      }

      encoder.reset_nal();
    }
  } );

  auto cull_needed = make_shared<bool>( false );

  loop.add_rule( "accept connection", web_listen_socket, Direction::In, [&] {
    clients.emplace_back( categories, web_listen_socket, loop, cull_needed );
  } );

  while ( loop.wait_next_event( -1 ) != EventLoop::Result::Exit ) {
  }

  return EXIT_SUCCESS;
}
