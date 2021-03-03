#pragma once

#include <ostream>

#include "compositor.hh"
#include "crypto.hh"
#include "eventloop.hh"
#include "keys.hh"
#include "socket.hh"
#include "summarize.hh"
#include "videofile.hh"
#include "vsclient.hh"

struct CompositingGroup
{
  std::string name_;
  Compositor compositor_ {};
  H264Encoder feed_ { 1280, 720, 24, "veryfast", "zerolatency" };
  Address destination_ { Address::abstract_unix( "stagecast-" + name_ + "-video" ) };
  Address destination2_ { Address::abstract_unix( "stagecast-" + name_ + "-video-filmout" ) };
  UnixDatagramSocket broadcast_socket_ {};
  Scene scene_ {};
  RasterRGBA composite_ { 1280, 720 };
  ColorspaceConverter converter_ { 1280, 720 };
  RasterYUV420 output_ { 1280, 720 };

  CompositingGroup( const std::string_view name )
    : name_( name )
  {
    broadcast_socket_.set_blocking( false );
  }

  void composite_and_send()
  {
    compositor_.apply( scene_, composite_ );
    converter_.convert( composite_, output_ );
    feed_.encode( output_ );

    if ( feed_.has_nal() ) {
      broadcast_socket_.sendto_ignore_errors(
        destination_, { reinterpret_cast<const char*>( feed_.nal().NAL.data() ), feed_.nal().NAL.size() } );
      broadcast_socket_.sendto_ignore_errors(
        destination2_, { reinterpret_cast<const char*>( feed_.nal().NAL.data() ), feed_.nal().NAL.size() } );
      feed_.reset_nal();
    }
  }
};

class VideoServer : public Summarizable
{
  static constexpr uint64_t CLIENT_TIMEOUT_NS = 4'000'000'000;

  UDPSocket socket_;
  uint64_t global_ns_timestamp_at_creation_;
  uint64_t server_clock() const;

  void receive_keyrequest( const Address& src, const Ciphertext& ciphertext );

  uint8_t num_clients_;
  uint64_t next_ack_ts_;

  std::vector<KnownVideoClient> clients_ {};

  struct Stats
  {
    unsigned int bad_packets;
  } stats_ {};

  RasterYUV420 default_raster_ { 1280, 720 };
  std::shared_ptr<RasterRGBA> default_raster_keyed_ = std::make_shared<RasterRGBA>( 1280, 720 );
  H264Encoder camera_feed_ { 1280, 720, 24, "veryfast", "zerolatency" };
  uint8_t camera_feed_live_no_ {};
  Address camera_destination_ { Address::abstract_unix( "stagecast-camera-video" ) };
  Address camera_destination2_ { Address::abstract_unix( "stagecast-camera-video-filmout" ) };
  UnixDatagramSocket camera_broadcast_socket_ {};

  uint64_t output_frames_encoded_ {};

  CompositingGroup preview_ { "preview" };
  CompositingGroup program_ { "program" };

  void summary( std::ostream& out ) const override;

  void load_cameras( Scene& compositor );

public:
  VideoServer( const uint8_t num_clients, EventLoop& loop );
  void add_key( const LongLivedKey& key );

  void set_live( const std::string_view name );

  void set_zoom( const video_control& control );

  void initialize_clock();

  void json_summary( Json::Value& root ) const;

  void insert_preview_layer( Layer&& layer );
  void remove_preview_layer( const std::string_view name ) { preview_.scene_.remove( name ); }
};
