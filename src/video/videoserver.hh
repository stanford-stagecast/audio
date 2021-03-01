#pragma once

#include <ostream>

#include "compositor.hh"
#include "crypto.hh"
#include "eventloop.hh"
#include "keys.hh"
#include "socket.hh"
#include "summarize.hh"
#include "vsclient.hh"

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
  H264Encoder camera_feed_ { 1280, 720, 24, "veryfast", "zerolatency" };
  uint8_t camera_feed_live_no_ {};
  Address camera_destination_ { Address::abstract_unix( "stagecast-camera-video" ) };
  UnixDatagramSocket camera_broadcast_socket_ {};

  RasterRGBA scratch_ { 1280, 720 };
  ColorspaceConverter converter_ { 1280, 720 };

  H264Encoder preview_feed_ { 1280, 720, 24, "veryfast", "zerolatency" };
  Address preview_destination_ { Address::abstract_unix( "stagecast-preview-video" ) };
  UnixDatagramSocket preview_broadcast_socket_ {};
  Scene preview_scene_ {};
  RasterRGBA preview_composite_ { 1280, 720 };
  RasterYUV420 preview_output_ { 1280, 720 };

  H264Encoder program_feed_ { 1280, 720, 24, "veryfast", "zerolatency" };
  Address program_destination_ { Address::abstract_unix( "stagecast-program-video" ) };
  UnixDatagramSocket program_broadcast_socket_ {};
  Scene program_scene_ {};
  RasterRGBA program_composite_ { 1280, 720 };
  RasterYUV420 program_output_ { 1280, 720 };

  void summary( std::ostream& out ) const override;

  void load_cameras( Compositor& compositor );

public:
  VideoServer( const uint8_t num_clients, EventLoop& loop );
  void add_key( const LongLivedKey& key );

  void set_live( const std::string_view name );

  void set_zoom( const video_control& control );

  void initialize_clock();

  void json_summary( Json::Value& root ) const;
};
