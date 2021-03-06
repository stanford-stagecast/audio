#include "eventloop.hh"
#include "h264_encoder.hh"
#include "opus.hh"
#include "socket.hh"
#include "stackbuffer.hh"
#include "videomkvwriter.hh"

#include <iostream>
#include <unistd.h>

using namespace std;

int main( int argc, char* argv[] )
{
  if ( argc <= 0 ) {
    abort();
  }

  if ( argc != 2 ) {
    cerr << "Usage: " << argv[0] << " video_name\n";
    return EXIT_FAILURE;
  }

  H264Encoder blank_frame_encoder { 1280, 720, 24, "veryfast", "zerolatency" };
  RasterYUV420 default_raster { 1280, 720 };
  blank_frame_encoder.encode( default_raster );
  if ( not blank_frame_encoder.has_nal() ) {
    throw runtime_error( "failed to encode default IDR" );
  }

  VideoMKVWriter muxer { 96000, 48000, 2, 24, 1280, 720, blank_frame_encoder.nal().NAL };

  UnixDatagramSocket audio_receiver {}, video_receiver {};
  audio_receiver.set_blocking( false );
  video_receiver.set_blocking( false );

  string video_stream_name = "stagecast-"s + argv[1] + "-video-filmout";

  audio_receiver.bind( Address::abstract_unix( "stagecast-program-audio-filmout" ) );
  video_receiver.bind( Address::abstract_unix( "stagecast-preview-video-filmout" ) );

  EventLoop loop;
  StackBuffer<0, uint32_t, 1048576> buf;

  uint64_t audio_time {}, video_time {};
  uint32_t video_frame_count {};

  loop.add_rule( "new audio segment", audio_receiver, Direction::In, [&] {
    buf.resize( audio_receiver.recv( buf.mutable_buffer() ) );
    audio_time += muxer.write_audio( buf, big_opus_frame::NUM_SAMPLES );
  } );

  loop.add_rule( "new video segment", video_receiver, Direction::In, [&] {
    buf.resize( video_receiver.recv( buf.mutable_buffer() ) );
    video_time += muxer.write_video( buf, video_frame_count, video_frame_count );
    video_frame_count++;
  } );

  FileDescriptor output { STDOUT_FILENO };

  loop.add_rule( "write output", output, Direction::Out, [&] { muxer.output().pop_to_fd( output ); } );

  while ( loop.wait_next_event( -1 ) != EventLoop::Result::Exit ) {
  }

  return EXIT_SUCCESS;
}
