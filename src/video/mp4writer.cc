#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <iostream>

#include "mp4writer.hh"

using namespace std;

int MP4Writer::av_check( const int retval )
{
  static array<char, 256> errbuf;

  if ( retval < 0 ) {
    if ( av_strerror( retval, errbuf.data(), errbuf.size() ) < 0 ) {
      throw runtime_error( "av_strerror: error code not found" );
    }

    errbuf.back() = 0;

    throw runtime_error( "libav error: " + string( errbuf.data() ) );
  }

  return retval;
}

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
  , output_ { CheckSystemCall( "open( /tmp/test.mp4 )",
                               open( "/tmp/test.mp4", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR ) ) }
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
  video_stream_->codecpar->width = width_;
  video_stream_->codecpar->height = height_;
  video_stream_->codecpar->video_delay = 0;
  video_stream_->codecpar->initial_padding = 0;
  video_stream_->codecpar->trailing_padding = 0;

  AVDictionary* flags = nullptr;
  av_check( av_dict_set( &flags, "movflags", "empty_moov", 0 ) );

  /* now write the header */
  av_check( avformat_write_header( context_.get(), &flags ) );
  header_written_ = true;

  if ( video_stream_->time_base.num != 1 or video_stream_->time_base.den != MP4_TIMEBASE ) {
    throw runtime_error( "video stream time base mismatch: " + to_string( video_stream_->time_base.num ) + " "
                         + to_string( video_stream_->time_base.den ) );
  }

  avio_flush( context_->pb );
  buf_.pop_to_fd( output_ );
  //  init_.close();

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

void MP4Writer::write( span<uint8_t> nal, const int64_t pts, const int64_t dts )
{
  AVPacket packet {};
  packet.buf = nullptr;
  packet.pts = pts;
  packet.dts = dts;
  packet.data = nal.mutable_data();
  packet.size = nal.size();
  packet.stream_index = 0;
  packet.flags = AV_PKT_FLAG_KEY;
  packet.duration = MP4_TIMEBASE / frame_rate_;
  packet.pos = -1;

  av_check( av_write_frame( context_.get(), &packet ) );
  av_check( av_write_frame( context_.get(), nullptr ) );
  avio_flush( context_->pb );
  buf_.pop_to_fd( output_ );

  //  stream_socket_.sendto( stream_destination_, buf_.readable_region() );
  //  buf_.pop( buf_.readable_region().size() );
}
