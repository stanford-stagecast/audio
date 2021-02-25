#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <iostream>

#include "webmwriter.hh"

using namespace std;

int WebMWriter::av_check( const int retval )
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

WebMWriter::WebMWriter( const int bit_rate, const uint32_t sample_rate, const uint8_t num_channels )
  : audio_stream_()
  , header_written_( false )
  , sample_rate_( sample_rate )
{
  {
    AVFormatContext* tmp_context;
    av_check( avformat_alloc_output_context2( &tmp_context, nullptr, "webm", nullptr ) );
    notnull( "avformat_alloc_output_context2", tmp_context );
    context_.reset( tmp_context );
  }

  /* open internal buffer */
  buffer_.reset( static_cast<uint8_t*>( notnull( "av_malloc", av_malloc( BUF_SIZE ) ) ) );
  context_->pb
    = notnull( "avio_alloc_context",
               avio_alloc_context( buffer_.get(), BUF_SIZE, true, &buf_, nullptr, write_helper, nullptr ) );

  /* allocate audio stream */
  audio_stream_ = notnull( "avformat_new_stream", avformat_new_stream( context_.get(), nullptr ) );

  if ( audio_stream_ != context_->streams[0] ) {
    throw runtime_error( "unexpected stream index != 0" );
  }

  audio_stream_->time_base = { 1, WEBM_TIMEBASE };
  audio_stream_->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
  audio_stream_->codecpar->codec_id = AV_CODEC_ID_OPUS;
  audio_stream_->codecpar->bit_rate = bit_rate;
  audio_stream_->codecpar->bits_per_coded_sample = 16;
  audio_stream_->codecpar->channels = num_channels;
  audio_stream_->codecpar->sample_rate = sample_rate;
  audio_stream_->codecpar->initial_padding = 0;
  audio_stream_->codecpar->trailing_padding = 0;

  /* write OpusHead structure as private data -- required by https://wiki.xiph.org/MatroskaOpus,
     the unofficial Opus-in-WebM spec, and enforced by libnestegg (used by Firefox) */

  struct __attribute__( ( packed ) ) OpusHead
  {
    array<char, 8> signature = { 'O', 'p', 'u', 's', 'H', 'e', 'a', 'd' };
    uint8_t version = 1;
    uint8_t channels {};
    uint16_t pre_skip = htole16( 0 );
    uint32_t input_sample_rate {};
    uint16_t output_gain = htole16( 0 );
    uint8_t channel_mapping_family = 0;
  } opus_head;

  opus_head.channels = num_channels;
  opus_head.input_sample_rate = htole32( sample_rate );

  static_assert( sizeof( opus_head ) == 19 );

  audio_stream_->codecpar->extradata
    = reinterpret_cast<uint8_t*>( notnull( "av_malloc", av_malloc( 19 + AV_INPUT_BUFFER_PADDING_SIZE ) ) );
  audio_stream_->codecpar->extradata_size = 19;
  memcpy( audio_stream_->codecpar->extradata, &opus_head, sizeof( OpusHead ) );

  /* now write the header */
  av_check( avformat_write_header( context_.get(), nullptr ) );
  header_written_ = true;

  if ( audio_stream_->time_base.num != 1 or audio_stream_->time_base.den != WEBM_TIMEBASE ) {
    throw runtime_error( "audio stream time base mismatch" );
  }

  static constexpr char filename[] = "/tmp/stagecast-audio.init";
  FileDescriptor init_ { CheckSystemCall( "open( "s + filename + " )",
                                          open( filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR ) ) };

  avio_flush( context_->pb );
  buf_.pop_to_fd( init_ );
  init_.close();

  if ( buf_.readable_region().size() ) {
    throw runtime_error( "did not write entire init segment" );
  }
}

WebMWriter::~WebMWriter()
{
  try {
    if ( header_written_ ) {
      av_check( av_write_trailer( context_.get() ) );
    }
  } catch ( const exception& e ) {
    cerr << "Exception in WebMWriter destructor: " << e.what() << "\n";
  }
}

void WebMWriter::write( opus_frame& frame, const uint64_t starting_sample_number )
{
  AVPacket packet {};
  packet.buf = nullptr;
  packet.pts = WEBM_TIMEBASE * starting_sample_number / sample_rate_;
  packet.dts = WEBM_TIMEBASE * starting_sample_number / sample_rate_;
  packet.data = frame.mutable_unsigned_data_ptr();
  packet.size = frame.length();
  packet.stream_index = 0;
  packet.flags = AV_PKT_FLAG_KEY;
  packet.duration = WEBM_TIMEBASE * opus_frame::NUM_SAMPLES_MINLATENCY / sample_rate_;
  packet.pos = -1;

  av_check( av_write_frame( context_.get(), &packet ) );
  av_check( av_write_frame( context_.get(), nullptr ) );
  avio_flush( context_->pb );
  stream_socket_.sendto( stream_destination_, buf_.readable_region() );
  buf_.pop( buf_.readable_region().size() );
}
