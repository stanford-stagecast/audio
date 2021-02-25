#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <iostream>

#include "av_check.hh"
#include "mp4writer.hh"

using namespace std;

int write_helper( void* rb_opaque, uint8_t* buf, int buf_size )
{
  if ( buf_size <= 0 ) {
    throw runtime_error( "buf_size <= 0" );
  }
  const size_t u_buf_size = buf_size;
  RingBuffer* rb = reinterpret_cast<RingBuffer*>( notnull( "rb_opaque", rb_opaque ) );
  if ( rb->writable_region().length() < u_buf_size ) {
    throw runtime_error( "write_helper had no room to write" );
  }
  rb->push_from_const_str( { reinterpret_cast<const char*>( notnull( "buf", buf ) ), u_buf_size } );
  return buf_size;
}

MP4Writer::MP4Writer( const unsigned int frame_rate, const unsigned int width, const unsigned int height )
  : video_stream_()
  , header_written_( false )
  , frame_rate_( frame_rate )
  , width_( width )
  , height_( height )
{
  {
    AVFormatContext* tmp_context;
    av_check( avformat_alloc_output_context2( &tmp_context, nullptr, "mp4", nullptr ) );
    notnull( "avformat_alloc_output_context2", tmp_context );
    context_.reset( tmp_context );
  }

  /* open internal buffer */
  buffer_.reset( static_cast<uint8_t*>( notnull( "av_malloc", av_malloc( BUF_SIZE ) ) ) );
  context_->pb
    = notnull( "avio_alloc_context",
               avio_alloc_context( buffer_.get(), BUF_SIZE, true, &buf_, nullptr, write_helper, nullptr ) );

  /* allocate video stream */
  video_stream_ = notnull( "avformat_new_stream", avformat_new_stream( context_.get(), nullptr ) );

  if ( video_stream_ != context_->streams[0] ) {
    throw runtime_error( "unexpected stream index != 0" );
  }

  video_stream_->time_base = { 1, MP4_TIMEBASE };
  video_stream_->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
  video_stream_->codecpar->codec_id = AV_CODEC_ID_H264;
  video_stream_->codecpar->profile = FF_PROFILE_H264_HIGH;
  video_stream_->codecpar->level = FF_PROFILE_H264_HIGH;
  video_stream_->codecpar->width = width_;
  video_stream_->codecpar->height = height_;
  video_stream_->codecpar->sample_aspect_ratio = { 1, 1 };
  video_stream_->codecpar->color_space = AVCOL_SPC_BT709;
  video_stream_->codecpar->format = AV_PIX_FMT_YUV420P;
  video_stream_->codecpar->video_delay = 0;
  video_stream_->codecpar->initial_padding = 0;
  video_stream_->codecpar->trailing_padding = 0;

  AVDictionary* flags = nullptr;
  av_check( av_dict_set( &flags, "movflags", "empty_moov+default_base_moof", 0 ) );

  /* now write the header */
  av_check( avformat_write_header( context_.get(), &flags ) );
  header_written_ = true;

  if ( video_stream_->time_base.num != 1 or video_stream_->time_base.den != MP4_TIMEBASE ) {
    throw runtime_error( "video stream time base mismatch: " + to_string( video_stream_->time_base.num ) + " "
                         + to_string( video_stream_->time_base.den ) );
  }

  static constexpr char filename[] = "/tmp/stagecast-video.init";
  FileDescriptor init_ { CheckSystemCall( "open( "s + filename + " )",
                                          open( filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR ) ) };

  avio_flush( context_->pb );
  buf_.pop_to_fd( init_ );
  init_.close();

  if ( buf_.readable_region().size() ) {
    throw runtime_error( "did not write entire init segment" );
  }
}

MP4Writer::~MP4Writer()
{
  try {
    if ( header_written_ ) {
      av_check( av_write_trailer( context_.get() ) );
    }
  } catch ( const exception& e ) {
    cerr << "Exception in MP4Writer destructor: " << e.what() << "\n";
  }
}

void MP4Writer::write( const string_view nal, const uint32_t presentation_no, const uint32_t display_no )
{
  AVPacket packet {};
  packet.buf = nullptr;
  packet.pts = presentation_no * MP4_TIMEBASE / frame_rate_;
  packet.dts = display_no * MP4_TIMEBASE / frame_rate_;
  packet.data = const_cast<uint8_t*>(
    reinterpret_cast<const uint8_t*>( nal.data() ) ); /* hope that av_write_frame doesn't change contents */
  packet.size = nal.size();
  packet.stream_index = 0;
  packet.flags = AV_PKT_FLAG_KEY;
  packet.duration = MP4_TIMEBASE / frame_rate_;
  packet.pos = -1;

  av_check( av_write_frame( context_.get(), &packet ) );
  av_check( av_write_frame( context_.get(), nullptr ) );
  avio_flush( context_->pb );
}
