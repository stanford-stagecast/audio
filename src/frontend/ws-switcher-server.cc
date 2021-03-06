#include <cstdlib>
#include <iostream>

#include <csignal>

#include "control_messages.hh"
#include "eventloop.hh"
#include "ewma.hh"
#include "keys.hh"
#include "mmap.hh"
#include "mp4writer.hh"
#include "secure_socket.hh"
#include "socket.hh"
#include "stackbuffer.hh"
#include "stats_printer.hh"
#include "ws_server.hh"

using namespace std;
using namespace std::chrono;

template<class Message>
void send_control( const Message& message )
{
  StackBuffer<0, uint16_t, 65535> buf;
  Serializer s { buf.mutable_buffer() };
  s.integer( Message::id );
  s.object( message );
  buf.resize( s.bytes_written() );

  UDPSocket socket;
  socket.sendto( { "127.0.0.1", video_server_control_port() }, buf );
}

struct Scene
{
  string name {};

  list<insert_layer> layers {};

  static Scene iso_scene( const string_view name )
  {
    Scene ret;
    ret.name = name;
    {
      insert_layer inst;
      inst.name = name;
      inst.width = 1280;
      inst.z = 50;
      ret.layers.push_back( inst );
    }
    return ret;
  }

  static Scene two_shot( const string_view name1, const string_view name2 )
  {
    return two_shot( name1, name2, string( name1 ) + " | " + string( name2 ), {}, false );
  }

  static Scene two_shot( const string_view name1,
                         const string_view name2,
                         const string the_name,
                         const optional<string_view> underlay,
                         const bool inset = false )
  {
    Scene ret;
    ret.name = the_name;

    {
      insert_layer inst;
      inst.name = name1;
      inst.width = inset ? 620 : 640;
      inst.x = inset ? 20 : 0;
      inst.y = 180;
      inst.z = 50;
      ret.layers.push_back( inst );
    }

    {
      insert_layer inst;
      inst.name = name2;
      inst.width = inset ? 620 : 640;
      inst.x = 640;
      inst.y = 180;
      inst.z = 50;
      ret.layers.push_back( inst );
    }

    if ( underlay.has_value() ) {
      insert_layer inst;
      inst.name = underlay.value();
      inst.width = 1280;
      inst.x = 0;
      inst.y = 0;
      inst.z = 60;
      ret.layers.push_back( inst );
    }

    return ret;
  }

  static Scene three_shot( const string_view name1, const string_view name2, const string_view name3 )
  {
    return three_shot(
      name1, name2, name3, string( name1 ) + " | " + string( name2 ) + " | " + string( name3 ), {} );
  }

  static Scene three_shot( const string_view name1,
                           const string_view name2,
                           const string_view name3,
                           const string the_name,
                           const optional<string_view> underlay,
                           const bool inset = false )
  {
    Scene ret;
    ret.name = the_name;

    {
      insert_layer inst;
      inst.name = name1;
      inst.width = inset ? 413 : 427;
      inst.x = inset ? 20 : 0;
      inst.y = 240;
      inst.z = 50;
      ret.layers.push_back( inst );
    }

    {
      insert_layer inst;
      inst.name = name2;
      inst.width = inset ? 413 : 427;
      inst.x = inset ? ( 20 + 413 ) : 427;
      inst.y = 240;
      inst.z = 50;
      ret.layers.push_back( inst );
    }

    {
      insert_layer inst;
      inst.name = name3;
      inst.width = inset ? 413 : 427;
      inst.x = inset ? ( 20 + 413 + 413 ) : 427;
      inst.y = 240;
      inst.z = 50;
      ret.layers.push_back( inst );
    }

    if ( underlay.has_value() ) {
      insert_layer inst;
      inst.name = underlay.value();
      inst.width = 1280;
      inst.x = 0;
      inst.y = 0;
      inst.z = 60;
      ret.layers.push_back( inst );
    }

    return ret;
  }
};

struct SceneList
{
  vector<Scene> scenes {};

  optional<atomic_scene_update> make( const string_view scene_name ) const
  {
    optional<atomic_scene_update> ret;
    for ( auto& scene : scenes ) {
      if ( scene.name == scene_name ) {
        cerr << "found " << scene_name << "\n";

        ret.emplace();

        {
          remove_layer removal;
          memcpy( removal.name.mutable_data_ptr(), "all", strlen( "all" ) );
          removal.name.resize( strlen( "all" ) );
          ret->removals.push_back( removal );
        }

        for ( auto& layer : scene.layers ) {
          ret->insertions.push_back( layer );
        }

        break;
      }
    }

    return ret;
  }
};

void split_on_char( const string_view str, const char ch_to_find, vector<string_view>& ret )
{
  ret.clear();

  bool in_double_quoted_string = false;
  unsigned int field_start = 0; // start of next token
  for ( unsigned int i = 0; i < str.size(); i++ ) {
    const char ch = str[i];
    if ( ch == '"' ) {
      in_double_quoted_string = !in_double_quoted_string;
    } else if ( in_double_quoted_string ) {
      continue;
    } else if ( ch == ch_to_find ) {
      ret.emplace_back( str.substr( field_start, i - field_start ) );
      field_start = i + 1;
    }
  }

  ret.emplace_back( str.substr( field_start ) );
}

struct EventCategories
{
  size_t close, SSL_read, SSL_write, ws_handshake, ws_receive, ws_send;

  EventCategories( EventLoop& loop )
    : close( loop.add_category( "close" ) )
    , SSL_read( loop.add_category( "SSL_read" ) )
    , SSL_write( loop.add_category( "SSL_write" ) )
    , ws_handshake( loop.add_category( "WebSocket handshake" ) )
    , ws_receive( loop.add_category( "WebSocket receive" ) )
    , ws_send( loop.add_category( "WebSocket send" ) )
  {}
};

enum class stream_t : uint8_t
{
  Preview,
  Program
};

class ClientConnection
{
  shared_ptr<SceneList> scenes_;
  SSLSession ssl_session_;
  WebSocketServer ws_server_;
  MP4Writer muxer_ { 24, 1280, 720 };
  WebSocketFrame ws_frame_;
  stream_t the_stream_;

  uint32_t video_frame_count_ {};

  vector<EventLoop::RuleHandle> rules_;

  bool good_ = true;
  string error_text_ {};

  shared_ptr<bool> cull_needed_;

  unsigned int controls_sent_ {};

public:
  void cull( const string_view s )
  {
    *cull_needed_ = true;
    good_ = false;

    if ( not error_text_.empty() ) {
      error_text_ += " + "sv;
    }
    error_text_ += s;

    for ( auto& rule : rules_ ) {
      rule.cancel();
    }
    rules_.clear();
  }

private:
  float mean_buffer_ = 0.0;
  float last_buffer_ = 0.0;
  vector<string_view> fields_ {};
  void parse_message( const string_view s )
  {
    if ( ( s.size() > 7 ) and ( s.substr( 0, 7 ) == "buffer " ) ) {
      string_view num = s.substr( 7 );
      last_buffer_ = stof( string( num ) );
      ewma_update( mean_buffer_, last_buffer_, 0.05 );
    } else if ( ( s.size() > 6 ) and ( s.substr( 0, 6 ) == "scene " ) ) {
      cerr << "incoming: " << s << "\n";

      const string_view scene_name = s.substr( 6 );

      auto inst = scenes_->make( scene_name );
      if ( inst.has_value() ) {
        send_control( inst.value() );
      }
    }
  }

  bool skipping_ {};
  unsigned int frames_since_idr_ {};
  unsigned int idrs_since_skip_ {};

public:
  const TCPSocket& socket() const { return ssl_session_.socket(); }

  bool good() const { return good_; }

  void push_NAL( const string_view s, const stream_t stream )
  {
    if ( stream != the_stream_ ) {
      return;
    }

    if ( MP4Writer::is_idr( s ) ) {
      frames_since_idr_ = 0;
      skipping_ = false;
      idrs_since_skip_++;
    } else {
      frames_since_idr_++;
    }

    if ( skipping_ and frames_since_idr_ >= 47 ) {
      idrs_since_skip_ = 0;
    } else {
      muxer_.write( s, video_frame_count_, video_frame_count_ );
      video_frame_count_++;
    }
  }

  void push_update( const string_view str )
  {
    if ( ws_server_.handshake_complete()
         and ssl_session_.outbound_plaintext().writable_region().size()
               > str.size() + 1 + WebSocketFrame::max_overhead() ) {

      ws_frame_.payload.resize( 1 + str.size() );
      ws_frame_.payload.at( 0 ) = 3;
      memcpy( ws_frame_.payload.data() + 1, str.data(), str.size() );

      Serializer s { ssl_session_.outbound_plaintext().writable_region() };
      s.object( ws_frame_ );
      ssl_session_.outbound_plaintext().push( s.bytes_written() );
    }
  }

  bool can_send( const size_t len ) const
  {
    return ssl_session_.outbound_plaintext().writable_region().size() >= len;
  }

  ClientConnection( const EventCategories& categories,
                    const shared_ptr<SceneList> scenes,
                    SSLContext& context,
                    TCPSocket& listening_socket,
                    const string& origin,
                    EventLoop& loop,
                    shared_ptr<bool> cull_needed,
                    const stream_t the_stream )
    : scenes_( scenes )
    , ssl_session_( context.make_SSL_handle(),
                    [&] {
                      auto sock = listening_socket.accept();
                      sock.set_blocking( false );
                      sock.set_tcp_nodelay( true );
                      return sock;
                    }() )
    , ws_server_( origin )
    , ws_frame_()
    , the_stream_( the_stream )
    , rules_()
    , cull_needed_( cull_needed )
  {
    cerr << "New connection from " << ssl_session_.socket().peer_address().to_string() << "\n";

    rules_.reserve( 10 );

    rules_.push_back( loop.add_rule(
      categories.close,
      [this] { cull( "WebSocket closure or error" ); },
      [this] {
        return good() and ws_server_.should_close_connection()
               and ssl_session_.outbound_plaintext().readable_region().empty();
      } ) );

    rules_.push_back( loop.add_rule(
      categories.SSL_read,
      ssl_session_.socket(),
      Direction::In,
      [this] {
        try {
          ssl_session_.do_read();
        } catch ( const exception& e ) {
          cull( e.what() );
        }
      },
      [this] { return good() and ssl_session_.want_read(); },
      [this] { cull( "socket closed" ); } ) );

    rules_.push_back( loop.add_rule(
      categories.SSL_write,
      ssl_session_.socket(),
      Direction::Out,
      [this] {
        try {
          ssl_session_.do_write();
        } catch ( const exception& e ) {
          cull( e.what() );
        }
      },
      [this] { return good() and ssl_session_.want_write(); },
      [this] { cull( "socket closed" ); } ) );

    rules_.push_back( loop.add_rule(
      categories.ws_handshake,
      [this] { ws_server_.do_handshake( ssl_session_.inbound_plaintext(), ssl_session_.outbound_plaintext() ); },
      [this] {
        return good() and ( not ssl_session_.inbound_plaintext().readable_region().empty() )
               and ( not ws_server_.handshake_complete() );
      } ) );

    ws_frame_.fin = true;
    ws_frame_.opcode = WebSocketFrame::opcode_t::Binary;

    rules_.push_back( loop.add_rule(
      categories.ws_send,
      [this] {
        ws_frame_.payload.resize(
          min( muxer_.output().readable_region().size() + 1,
               ssl_session_.outbound_plaintext().writable_region().size() - WebSocketFrame::max_overhead() - 1 ) );
        ws_frame_.payload.at( 0 ) = 0;
        memcpy(
          ws_frame_.payload.data() + 1, muxer_.output().readable_region().data(), ws_frame_.payload.size() - 1 );

        muxer_.output().pop( ws_frame_.payload.size() - 1 );

        Serializer s { ssl_session_.outbound_plaintext().writable_region() };
        s.object( ws_frame_ );
        ssl_session_.outbound_plaintext().push( s.bytes_written() );
      },
      [&] {
        return ssl_session_.outbound_plaintext().writable_region().size()
                 > size_t( 1 + WebSocketFrame::max_overhead() )
               and muxer_.output().readable_region().size() and ws_server_.handshake_complete();
      } ) );

    rules_.push_back( loop.add_rule(
      categories.ws_send,
      [this] {
        ws_frame_.payload = " " + scenes_->scenes.at( controls_sent_ ).name;
        ws_frame_.payload.at( 0 ) = 2;

        Serializer s { ssl_session_.outbound_plaintext().writable_region() };
        s.object( ws_frame_ );
        ssl_session_.outbound_plaintext().push( s.bytes_written() );

        controls_sent_++;
      },
      [&] {
        return ws_server_.handshake_complete() and ( controls_sent_ < scenes_->scenes.size() )
               and ssl_session_.outbound_plaintext().writable_region().size() > 100;
      } ) );

    rules_.push_back( loop.add_rule(
      categories.ws_receive,
      [this] {
        ws_server_.endpoint().read( ssl_session_.inbound_plaintext(), ssl_session_.outbound_plaintext() );
        if ( ws_server_.endpoint().ready() ) {
          parse_message( ws_server_.endpoint().message() );

          if ( last_buffer_ > 0.11 and mean_buffer_ > 0.11 and idrs_since_skip_ > 0 ) {
            skipping_ = true;
          }

          //          cerr << skipping_ << " " << idrs_since_skip_ << " " << last_buffer_ << " " << mean_buffer_
          //          <<
          //          "\n";

          ws_server_.endpoint().pop_message();
        }
      },
      [this] { return good() and ssl_session_.inbound_plaintext().readable_region().size(); } ) );
  }

  ~ClientConnection()
  {
    if ( not error_text_.empty() ) {
      cerr << "Client error: " << error_text_ << "\n";
    }
  }

  SSLSession& session() { return ssl_session_; }

  ClientConnection( const ClientConnection& other ) noexcept = delete;
  ClientConnection& operator=( const ClientConnection& other ) noexcept = delete;

  ClientConnection( ClientConnection&& other ) noexcept = delete;
  ClientConnection& operator=( ClientConnection&& other ) noexcept = delete;
};

void program_body( const string origin, const string cert_filename, const string privkey_filename )
{
  ios::sync_with_stdio( false );

  if ( SIG_ERR == signal( SIGPIPE, SIG_IGN ) ) {
    throw unix_error( "signal" );
  }

  SSLServerContext ssl_context { cert_filename, privkey_filename };

  /* receive new additions to stream */
  UnixDatagramSocket preview_receiver;
  preview_receiver.set_blocking( false );
  preview_receiver.bind( Address::abstract_unix( "stagecast-preview-video" ) );

  UnixDatagramSocket program_receiver;
  program_receiver.set_blocking( false );
  program_receiver.bind( Address::abstract_unix( "stagecast-program-video" ) );

  /* receive new metadata  */
  /*
  UnixDatagramSocket json_receiver;
  json_receiver.set_blocking( false );
  json_receiver.bind( Address::abstract_unix( "stagecast-server-video-json" ) );
  */

  /* start listening for HTTP connections */
  TCPSocket preview_listen_socket;
  preview_listen_socket.set_reuseaddr();
  preview_listen_socket.set_blocking( false );
  preview_listen_socket.set_tcp_nodelay( true );
  preview_listen_socket.bind( { "0", 8401 } );
  preview_listen_socket.listen();

  /* start listening for HTTP connections */
  TCPSocket program_listen_socket;
  program_listen_socket.set_reuseaddr();
  program_listen_socket.set_blocking( false );
  program_listen_socket.set_tcp_nodelay( true );
  program_listen_socket.bind( { "0", 8402 } );
  program_listen_socket.listen();

  struct ClientList : public Summarizable
  {
    list<ClientConnection> clients {};

    virtual void summary( ostream& out ) const override
    {
      out << "Connections: ";
      unsigned int i = 0;
      auto it = clients.begin();
      while ( it != clients.end() ) {
        out << "   " << i << ":\t" << it->socket().peer_address().to_string() << "\n";
        i++;
        it++;
      }
    }
  };
  auto clients = make_shared<ClientList>();

  /* set up event loop */
  auto loop = make_shared<EventLoop>();
  EventCategories categories { *loop };

  auto cull_needed = make_shared<bool>( false );

  StackBuffer<0, uint32_t, 1048576> buf;

  loop->add_rule( "new preview segment", preview_receiver, Direction::In, [&] {
    buf.resize( preview_receiver.recv( buf.mutable_buffer() ) );

    for ( auto& client : clients->clients ) {
      try {
        client.push_NAL( buf, stream_t::Preview );
      } catch ( const exception& e ) {
        cerr << "Muxer exception: " << e.what() << "\n";
        client.cull( "Muxer exception" );
      }
    }
  } );

  loop->add_rule( "new program segment", program_receiver, Direction::In, [&] {
    buf.resize( program_receiver.recv( buf.mutable_buffer() ) );

    for ( auto& client : clients->clients ) {
      try {
        client.push_NAL( buf, stream_t::Program );
      } catch ( const exception& e ) {
        cerr << "Muxer exception: " << e.what() << "\n";
        client.cull( "Muxer exception" );
      }
    }
  } );

  auto scenes = make_shared<SceneList>();

  scenes->scenes.push_back( Scene::iso_scene( "QLab" ) );
  scenes->scenes.push_back( Scene::iso_scene( "Audrey" ) );
  scenes->scenes.push_back( Scene::iso_scene( "Aiyana" ) );
  scenes->scenes.push_back( Scene::iso_scene( "JJ" ) );
  scenes->scenes.push_back( Scene::iso_scene( "Justine" ) );
  scenes->scenes.push_back( Scene::iso_scene( "Sam" ) );
  scenes->scenes.push_back( Scene::iso_scene( "Michael" ) );
  scenes->scenes.push_back( Scene::iso_scene( "Keith" ) );

  scenes->scenes.push_back( Scene::iso_scene( "Gelsey" ) );
  scenes->scenes.push_back( Scene::iso_scene( "Mariel" ) );
  scenes->scenes.push_back( Scene::iso_scene( "Josh" ) );

  scenes->scenes.push_back( Scene::two_shot( "Michael", "Keith" ) );

  scenes->scenes.push_back( Scene::two_shot( "Audrey", "JJ", "TV Pilot 1", "QLab" ) );
  scenes->scenes.push_back( Scene::two_shot( "Audrey", "Aiyana", "TV Pilot 2", "QLab" ) );
  scenes->scenes.push_back( Scene::two_shot( "Audrey", "Justine", "TV Pilot 3", "QLab" ) );
  scenes->scenes.push_back( Scene::two_shot( "Audrey", "Sam", "TV Pilot 4", "QLab" ) );

  scenes->scenes.push_back( Scene::two_shot( "JJ", "Aiyana" ) );

  {
    Scene lawyer = Scene::iso_scene( "JJ" );
    lawyer.name = "Unboxing: JJ+Aiyana in corner";

    insert_layer overlay;
    overlay.x = 853;
    overlay.y = 0;
    overlay.width = 427;
    overlay.z = 40;
    overlay.name = NetString( "Aiyana" );

    lawyer.layers.push_back( overlay );

    scenes->scenes.push_back( lawyer );
  }

  scenes->scenes.push_back( Scene::two_shot( "JJ", "Audrey" ) );

  scenes->scenes.push_back( Scene::two_shot( "Justine", "Sam", "Unbox Bros. (Justine+Sam)", "QLab", true ) );
  scenes->scenes.push_back(
    Scene::three_shot( "Justine", "Audrey", "Sam", "Unbox Bros. (Justine/Audrey/Sam)", "QLab", true ) );

  {
    Scene band;
    band.name = "Band";

    {
      insert_layer layer;
      layer.name = NetString( "Gelsey" );
      layer.x = 320;
      layer.y = 0;
      layer.width = 1280 / 2;
      layer.z = 50;
      band.layers.push_back( layer );
    }

    {
      insert_layer layer;
      layer.name = NetString( "Josh" );
      layer.x = 0;
      layer.y = 720 / 2;
      layer.width = 1280 / 2;
      layer.z = 50;
      band.layers.push_back( layer );
    }

    {
      insert_layer layer;
      layer.name = NetString( "Mariel" );
      layer.x = 1280 / 2;
      layer.y = 720 / 2;
      layer.width = 1280 / 2;
      layer.z = 50;
      band.layers.push_back( layer );
    }

    scenes->scenes.push_back( band );
  }

  {
    Scene loup;
    loup.name = "Loup Garou";

    {
      insert_layer layer;
      layer.name = NetString( "JJ" );
      layer.x = 5;
      layer.y = 180;
      layer.width = 630;
      layer.z = 50;
      loup.layers.push_back( layer );
    }

    {
      insert_layer layer;
      layer.name = NetString( "Aiyana" );
      layer.x = 645;
      layer.y = 180;
      layer.width = 630;
      layer.z = 50;
      loup.layers.push_back( layer );
    }

    {
      insert_layer inst;
      inst.name = NetString( "QLab" );
      inst.width = 1280;
      inst.x = 0;
      inst.y = 0;
      inst.z = 60;
      loup.layers.push_back( inst );
    }

    scenes->scenes.push_back( loup );
  }

  {
    Scene curtain;
    curtain.name = "Curtain Call";
    list<string> names = { "Audrey", "Aiyana", "JJ", "Justine", "Sam", "xxx", "Mariel", "Gelsey", "Josh" };

    for ( unsigned int y = 0; y < 3; y++ ) {
      for ( unsigned int x = 0; x < 3; x++ ) {
        if ( names.front() != "xxx" ) {
          insert_layer layer;
          layer.name = NetString( names.front() );
          layer.x = x * 427;
          layer.y = y * 240;
          layer.width = 427;
          layer.z = 50;
          curtain.layers.push_back( layer );
          names.pop_front();
        } else {
          {
            insert_layer layer;
            layer.name = NetString( "Michael" );
            layer.x = x * 427;
            layer.y = y * 240;
            layer.width = 427 / 2;
            layer.z = 50;
            curtain.layers.push_back( layer );
          }

          {
            insert_layer layer;
            layer.name = NetString( "Michael" );
            layer.x = x * 427 + ( 427 / 2 );
            layer.y = y * 240 + ( 240 / 2 );
            layer.width = 427 / 2;
            layer.z = 50;
            curtain.layers.push_back( layer );
          }
          {
            insert_layer layer;
            layer.name = NetString( "Keith" );
            layer.x = x * 427 + ( 427 / 2 );
            layer.y = y * 240;
            layer.width = 427 / 2;
            layer.z = 50;
            curtain.layers.push_back( layer );
          }
          {
            insert_layer layer;
            layer.name = NetString( "Keith" );
            layer.x = x * 427;
            layer.y = y * 240 + ( 240 / 2 );
            layer.width = 427 / 2;
            layer.z = 50;
            curtain.layers.push_back( layer );
          }
          names.pop_front();
        }
      }
    }

    scenes->scenes.push_back( curtain );
  }

  loop->add_rule( "new Preview connection", preview_listen_socket, Direction::In, [&] {
    clients->clients.emplace_back(
      categories, scenes, ssl_context, preview_listen_socket, origin, *loop, cull_needed, stream_t::Preview );
  } );

  loop->add_rule( "new Program connection", program_listen_socket, Direction::In, [&] {
    clients->clients.emplace_back(
      categories, scenes, ssl_context, program_listen_socket, origin, *loop, cull_needed, stream_t::Program );
  } );

  /* cull old connections */
  loop->add_rule(
    "cull connections",
    [&] {
      clients->clients.remove_if( []( const ClientConnection& x ) { return not x.good(); } );
      *cull_needed = false;
    },
    [&cull_needed] { return *cull_needed; } );

  /*
  StatsPrinterTask stats_printer { loop };
  stats_printer.add( clients );
  */

  while ( loop->wait_next_event( 500 ) != EventLoop::Result::Exit ) {
  }
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort();
    }
    if ( argc != 4 ) {
      cerr << "Usage: " << argv[0] << " origin certificate private_key\n";
      return EXIT_FAILURE;
    }

    program_body( argv[1], argv[2], argv[3] );
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
