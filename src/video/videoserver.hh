#pragma once

#include <ostream>

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

  void summary( std::ostream& out ) const override;

public:
  VideoServer( const uint8_t num_clients, EventLoop& loop );
  void add_key( const LongLivedKey& key );

  void set_live( const std::string_view name );

  void initialize_clock();
};
