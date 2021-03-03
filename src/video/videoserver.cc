#include "videoserver.hh"
#include "address.hh"

#include <chrono>
#include <iostream>
#include <thread>

using namespace std;
using namespace std::chrono;

uint64_t VideoServer::server_clock() const
{
  return ( Timer::timestamp_ns() - global_ns_timestamp_at_creation_ ) * 24 / 1'000'000'000;
}

void VideoServer::receive_keyrequest( const Address& src, const Ciphertext& ciphertext )
{
  /* decrypt */
  for ( auto& client : clients_ ) {
    if ( client.try_keyrequest( src, ciphertext, socket_ ) ) {
      return;
    }
  }
  stats_.bad_packets++;
}

void VideoServer::add_key( const LongLivedKey& key )
{
  const uint8_t next_id = clients_.size() + 1;
  const uint8_t ch1 = 2 * clients_.size();
  const uint8_t ch2 = ch1 + 1;
  clients_.emplace_back( next_id, key );
  cerr << "Added key #" << int( next_id ) << " for: " << key.name() << " on channels " << int( ch1 ) << ":"
       << int( ch2 ) << "\n";
}

VideoServer::VideoServer( const uint8_t num_clients, EventLoop& loop )
  : socket_()
  , global_ns_timestamp_at_creation_( Timer::timestamp_ns() )
  , num_clients_( num_clients )
  , next_ack_ts_ { Timer::timestamp_ns() }
{
  VideoFile default_raster_file { "/home/media/files/decoded/default.png.rawvideo" };
  default_raster_ = default_raster_file.raster();

  ColorspaceConverter converter_ { 1280, 720 };
  converter_.convert( default_raster_, *default_raster_keyed_ );

  socket_.set_blocking( false );
  camera_broadcast_socket_.set_blocking( false );
  socket_.bind( { "0", 9201 } );

  loop.add_rule( "network receive", socket_, Direction::In, [&] {
    Address src { nullptr, 0 };
    Ciphertext ciphertext;
    ciphertext.resize( socket_.recv( src, ciphertext.mutable_buffer() ) );
    if ( ciphertext.length() > 24 ) {
      const uint8_t node_id = ciphertext.as_string_view().back();
      if ( node_id == uint8_t( KeyMessage::keyreq_id ) ) {
        receive_keyrequest( src, ciphertext );
      } else if ( node_id > 0 and node_id <= clients_.size() ) {
        clients_.at( node_id - 1 ).receive_packet( src, ciphertext, server_clock() );
      } else {
        stats_.bad_packets++;
      }
    } else {
      stats_.bad_packets++;
    }
  } );

  loop.add_rule(
    "send acks",
    [&] {
      const uint64_t ts_now = Timer::timestamp_ns();

      for ( auto& client : clients_ ) {
        if ( client ) {
          client.client().send_packet( socket_ );

          if ( client.client().connection().sender_stats().last_good_ack_ts + CLIENT_TIMEOUT_NS < ts_now ) {
            client.clear_current_session();
          }
        }
      }
      next_ack_ts_ = Timer::timestamp_ns() + 5'000'000;
    },
    [&] { return Timer::timestamp_ns() > next_ack_ts_; } );

  loop.add_rule(
    "encode [camera]",
    [&] {
      RasterYUV420& output = clients_.at( camera_feed_live_no_ )
                               ? clients_.at( camera_feed_live_no_ ).client().raster()
                               : default_raster_;
      camera_feed_.encode( output );
      if ( camera_feed_.has_nal() ) {
        camera_broadcast_socket_.sendto_ignore_errors(
          camera_destination_,
          { reinterpret_cast<const char*>( camera_feed_.nal().NAL.data() ), camera_feed_.nal().NAL.size() } );
        camera_feed_.reset_nal();
      }
    },
    [&] { return server_clock() >= camera_feed_.frames_encoded() and not camera_feed_.has_nal(); } );

  loop.add_rule(
    "encode [preview & program]",
    [&] {
      load_cameras( preview_.scene_ );
      load_cameras( program_.scene_ );

      vector<thread> threads;
      threads.reserve( 2 );
      threads.emplace_back( [&] { preview_.composite_and_send(); } );
      threads.emplace_back( [&] { program_.composite_and_send(); } );
      for ( auto& x : threads ) {
        x.join();
      }
      output_frames_encoded_++;
    },
    [&] { return server_clock() >= output_frames_encoded_; } );

  preview_.scene_.insert( Layer { Layer::layer_type::Camera, "Sam", "", 0, 0, 640, 20 } );
  preview_.scene_.insert( Layer { Layer::layer_type::Camera, "Audrey", "", 640, 0, 640, 20 } );
  preview_.scene_.insert( Layer { Layer::layer_type::Camera, "JJ", "", 0, 360, 640, 20 } );
  preview_.scene_.insert( Layer { Layer::layer_type::Camera, "Justine", "", 640, 360, 640, 20 } );

  /* preview_.scene_.layers.emplace_back( "KeithBox", 640, 0, 640, false );
  preview_.scene_.layers.emplace_back( "KeithBox", 0, 360, 640, false );
  preview_.scene_.layers.emplace_back( "KeithBox", 640, 360, 640, false );
  */
}

void VideoServer::summary( ostream& out ) const
{
  out << "bad packets: " << stats_.bad_packets;
  out << " camera frames encoded: " << camera_feed_.frames_encoded();
  out << " live now: "
      << ( clients_.at( camera_feed_live_no_ ) ? clients_.at( camera_feed_live_no_ ).name()
                                               : "none " + to_string( camera_feed_live_no_ ) );
  out << "\n";
  for ( const auto& client : clients_ ) {
    if ( client ) {
      out << "#" << int( client.client().peer_id() ) << ": ";
      client.summary( out );
    }
  }
}

void VideoServer::json_summary( Json::Value& root ) const
{
  const auto& client = clients_.at( camera_feed_live_no_ );
  if ( client ) {
    root["live"] = client.name();
    root["zoom"]["x"] = client.client().zoom_.x;
    root["zoom"]["y"] = client.client().zoom_.y;
    root["zoom"]["zoom"] = float( 3840.0 ) / float( client.client().zoom_.width );

    root["crop"]["left"] = client.client().zoom_.crop_left;
    root["crop"]["right"] = client.client().zoom_.crop_right;
    root["crop"]["top"] = client.client().zoom_.crop_top;
    root["crop"]["bottom"] = client.client().zoom_.crop_bottom;
  } else {
    root["live"] = "none";
    root["zoom"]["x"] = 0;
    root["zoom"]["y"] = 0;
    root["zoom"]["zoom"] = 0;

    root["crop"]["left"] = 0;
    root["crop"]["right"] = 0;
    root["crop"]["top"] = 0;
    root["crop"]["bottom"] = 0;
  }
}

void VideoServer::set_live( const string_view name )
{
  for ( unsigned int i = 0; i < clients_.size(); i++ ) {
    if ( clients_.at( i ).name() == name ) {
      camera_feed_live_no_ = i;
    }
  }
}

bool valid( const video_control& c )
{
  if ( c.x >= 3840 ) {
    return false;
  }

  if ( c.y >= 2160 ) {
    return false;
  }

  if ( c.x + c.width > 3840 ) {
    return false;
  }

  if ( c.y + c.height > 2160 ) {
    return false;
  }

  if ( c.width == 0 or c.height == 0 ) {
    return false;
  }

  return true;
}

void VideoServer::set_zoom( const video_control& control )
{
  if ( not clients_.at( camera_feed_live_no_ ) ) {
    return;
  }

  auto& client = clients_.at( camera_feed_live_no_ ).client();

  auto& existing_zoom = client.zoom_;

  static constexpr uint16_t NEG1 = -1;

  if ( control.x != NEG1 ) {
    existing_zoom.x = min( control.x, uint16_t( 3840 ) );

    if ( not valid( existing_zoom ) ) {
      existing_zoom.x = 3840 - existing_zoom.width;
    }
  } else if ( control.y != NEG1 ) {
    existing_zoom.y = min( control.y, uint16_t( 2160 ) );

    if ( not valid( existing_zoom ) ) {
      existing_zoom.y = 2160 - existing_zoom.height;
    }
  } else if ( control.width != NEG1 ) {
    const uint16_t old_midpoint_x = existing_zoom.x + ( existing_zoom.width / 2 );
    const uint16_t old_midpoint_y = existing_zoom.y + ( existing_zoom.height / 2 );

    existing_zoom.width = max( uint16_t( 1280 ), min( control.width, uint16_t( 3840 ) ) );
    existing_zoom.height = 2160 * existing_zoom.width / 3840;

    if ( old_midpoint_x > existing_zoom.width / 2 ) {
      existing_zoom.x = old_midpoint_x - existing_zoom.width / 2;
    } else {
      existing_zoom.x = 0;
    }

    if ( old_midpoint_y > existing_zoom.height / 2 ) {
      existing_zoom.y = old_midpoint_y - existing_zoom.height / 2;
    } else {
      existing_zoom.y = 0;
    }

    if ( not valid( existing_zoom ) ) {
      existing_zoom.x = 3840 - existing_zoom.width;
      existing_zoom.y = 2160 - existing_zoom.height;
    }
  } else if ( control.crop_left != NEG1 ) {
    existing_zoom.crop_left = min( control.crop_left, uint16_t( 3840 ) );
  } else if ( control.crop_right != NEG1 ) {
    existing_zoom.crop_right = min( control.crop_right, uint16_t( 3840 ) );
  } else if ( control.crop_top != NEG1 ) {
    existing_zoom.crop_top = min( control.crop_top, uint16_t( 2160 ) );
  } else if ( control.crop_bottom != NEG1 ) {
    existing_zoom.crop_bottom = min( control.crop_bottom, uint16_t( 2160 ) );
  }

  if ( not valid( existing_zoom ) ) {
    cerr << "Warning, failed to correct zoom input.\n";
    existing_zoom.x = 0;
    existing_zoom.y = 0;
    existing_zoom.width = 3840;
    existing_zoom.height = 2160;
    existing_zoom.crop_left = 0;
    existing_zoom.crop_right = 0;
    existing_zoom.crop_top = 0;
    existing_zoom.crop_bottom = 0;
  }
}

void VideoServer::load_cameras( Scene& scene )
{
  for ( const auto& client : clients_ ) {
    if ( client ) {
      scene.load_camera_image( client.name(), client.client().raster_keyed_ );
    } else {
      scene.load_camera_image( client.name(), default_raster_keyed_ );
    }
  }
}

void VideoServer::insert_preview_layer( Layer&& layer )
{
  if ( layer.type == Layer::layer_type::Media ) {
    try {
      const string filename = "/home/media/files/decoded/" + layer.filename;
      layer.video = make_shared<VideoFile>( filename );
      preview_.scene_.insert( move( layer ) );

      cerr << "Successfully loaded " + filename + "\n";
    } catch ( const exception& e ) {
      cerr << e.what() << "\n";
    }
  } else {
    preview_.scene_.insert( move( layer ) );
  }
}
