#include <cstdlib>
#include <iostream>
#include <queue>

#include "alsa_devices.hh"
#include "audio_device_claim.hh"
#include "eventloop.hh"

#include "audio_task.hh"
#include "encoder_task.hh"
#include "stats_printer.hh"

#include "connection.hh"

using namespace std;

class TestEncoder : public OpusEncoderProcess
{
  AudioChannel channel { 4096 };

public:
  TestEncoder()
    : OpusEncoderProcess( 128000, 48000 )
  {}

  void encode( const unsigned int i )
  {
    if ( not enc1_.can_encode_frame( i * 120 ) ) {
      throw runtime_error( "can't encode frame " + to_string( i ) );
    }
    enc1_.encode_one_frame( channel );
    enc2_.encode_one_frame( channel );
    channel.pop( 120 );
  }
};

void program_body()
{
  TestEncoder encoder;

  NetworkSender sender1, sender2;
  NetworkReceiver receiver1, receiver2;

  queue<optional<Packet>> forward, reverse;

  for ( unsigned int i = 0; i < 30; i++ ) {
    forward.emplace();
    reverse.emplace();
  }

  for ( unsigned int i = 0; i < 1024; i++ ) {
    cout << "= = = = = " << i << " = = = = =\n";

    encoder.encode( i + 1 );
    sender1.push_frame( encoder );

    {
      Packet pack;
      sender1.set_sender_section( pack.sender_section );
      receiver1.set_receiver_section( pack.receiver_section );

      if ( i % 11 ) {
        forward.push( pack );
      } else {
        cout << "Discarding forward packet " << i << "\n";
        forward.emplace();
      }
    }

    if ( not forward.empty() ) {
      if ( forward.front().has_value() ) {
        const auto& pack = forward.front().value();
        sender2.receive_receiver_section( pack.receiver_section );
        receiver2.receive_sender_section( pack.sender_section );
        receiver2.pop_frames( receiver2.next_frame_needed() - receiver2.frames().range_begin() );
      }
      forward.pop();
    }

    {
      Packet reply;
      sender2.set_sender_section( reply.sender_section );
      receiver2.set_receiver_section( reply.receiver_section );

      if ( i % 13 ) {
        reverse.push( reply );
      } else {
        cout << "Discarding reverse packet " << i << "\n";
        reverse.emplace();
      }
    }

    if ( not reverse.empty() ) {
      if ( reverse.front().has_value() ) {
        const auto& reply = reverse.front().value();
        sender1.receive_receiver_section( reply.receiver_section );
        receiver1.receive_sender_section( reply.sender_section );
        receiver1.pop_frames( receiver1.next_frame_needed() - receiver1.frames().range_begin() );
      }
      reverse.pop();
    }

    {
      sender1.summary( cout );
      receiver1.summary( cout );
      sender2.summary( cout );
      receiver2.summary( cout );
    }
  }
}

int main()
{
  try {
    ios::sync_with_stdio( false );
    program_body();
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << "\n";
    global_timer().summary( cerr );
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
