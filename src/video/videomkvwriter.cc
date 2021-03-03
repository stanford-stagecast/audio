#include <iostream>

#include "videomkvwriter.hh"

using namespace std;

static int av_check( const int retval )
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

VideoMKVWriter::VideoMKVWriter( const int audio_bit_rate,
                                const uint32_t audio_sample_rate,
                                const uint8_t audio_num_channels,
                                const unsigned int video_frame_rate,
                                const unsigned int width,
                                const unsigned int height )
  : audio_stream_()
  , video_stream_()
  , header_written_( false )
  , sample_rate_( audio_sample_rate )
  , frame_rate_( video_frame_rate )
  , width_( width )
  , height_( height )
{
  {
    AVFormatContext* tmp_context;
    av_check( avformat_alloc_output_context2( &tmp_context, nullptr, "webm", nullptr ) );
    notnull( "avformat_alloc_output_context2", tmp_context );
    context_.reset( tmp_context );
  }

  /* open internal buffer */
  buffer_ = static_cast<uint8_t*>( notnull( "av_malloc", av_malloc( BUF_SIZE ) ) );
  context_->pb = notnull( "avio_alloc_context",
                          avio_alloc_context( buffer_, BUF_SIZE, true, &buf_, nullptr, write_helper, nullptr ) );

  /* allocate audio stream */
  audio_stream_ = notnull( "avformat_new_stream", avformat_new_stream( context_.get(), nullptr ) );

  if ( audio_stream_ != context_->streams[0] ) {
    throw runtime_error( "unexpected stream index != 0" );
  }

  audio_stream_->time_base = { 1, MKV_TIMEBASE };
  audio_stream_->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
  audio_stream_->codecpar->codec_id = AV_CODEC_ID_OPUS;
  audio_stream_->codecpar->bit_rate = audio_bit_rate;
  audio_stream_->codecpar->bits_per_coded_sample = 16;
  audio_stream_->codecpar->channels = audio_num_channels;
  audio_stream_->codecpar->sample_rate = audio_sample_rate;
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

  opus_head.channels = audio_num_channels;
  opus_head.input_sample_rate = htole32( sample_rate );

  static_assert( sizeof( opus_head ) == 19 );

  audio_stream_->codecpar->extradata
    = reinterpret_cast<uint8_t*>( notnull( "av_malloc", av_malloc( 19 + AV_INPUT_BUFFER_PADDING_SIZE ) ) );
  audio_stream_->codecpar->extradata_size = 19;
  memcpy( audio_stream_->codecpar->extradata, &opus_head, sizeof( OpusHead ) );

  /* allocate video stream */
  video_stream_ = notnull( "avformat_new_stream", avformat_new_stream( context_.get(), nullptr ) );

  if ( video_stream_ != context_->streams[1] ) {
    throw runtime_error( "unexpected stream index != 1" );
  }

  video_stream_->time_base = { 1, MKV_TIMEBASE };
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
  video_stream_->duration = 0;

  AVDictionary* flags = nullptr;
  av_check( av_dict_set( &flags, "movflags", "default_base_moof+faststart+frag_custom", 0 ) );

  /* now write the header */
  av_check( avformat_write_header( context_.get(), &flags ) );

  if ( audio_stream_->time_base.num != 1 or audio_stream_->time_base.den != MKV_TIMEBASE ) {
    throw runtime_error( "audio stream time base mismatch: " + to_string( audio_stream_->time_base.num ) + "/"
                         + to_string( audio_stream_->time_base.den ) );
  }

  if ( video_stream_->time_base.num != 1 or video_stream_->time_base.den != MKV_TIMEBASE ) {
    throw runtime_error( "video stream time base mismatch: " + to_string( video_stream_->time_base.num ) + " "
                         + to_string( video_stream_->time_base.den ) );
  }

  avio_flush( context_->pb );
}

void VideoMKVWriter::write_audio( const std::string_view frame, const uint16_t num_samples )
{
  AVPacket packet {};
  packet.buf = nullptr;
  packet.pts = uint64_t( MKV_TIMEBASE ) * uint64_t( sample_count_ ) / uint64_t( sample_rate_ );
  packet.dts = uint64_t( MKV_TIMEBASE ) * uint64_t( sample_count_ ) / uint64_t( sample_rate_ );
  packet.data = const_cast<uint8_t*>(
    reinterpret_cast<const uint8_t*>( frame.data() ) ); /* hope that av_write_frame doesn't change contents */
  packet.size = frame.length();
  packet.stream_index = 0;
  packet.flags = AV_PKT_FLAG_KEY;
  packet.duration = uint64_t( MKV_TIMEBASE ) * uint64_t( num_samples ) / uint64_t( sample_rate_ );
  packet.pos = -1;

  av_check( av_write_frame( context_.get(), &packet ) );
  av_check( av_write_frame( context_.get(), nullptr ) );
  avio_flush( context_->pb );

  sample_count_ += num_samples;
}

bool VideoMKVWriter::is_idr( const string_view nal )
{
  if ( nal.at( 0 ) != 0 or nal.at( 1 ) != 0 or nal.at( 2 ) != 0 or nal.at( 3 ) != 1 ) {
    throw runtime_error( "invalid NALU" );
  }

  if ( nal.at( 4 ) == 0x67 or nal.at( 4 ) == 0x68 ) {
    return true;
  }

  return false;
}

void VideoMKVWriter::write_video( const std::string_view nal,
                                  const uint32_t presentation_no,
                                  const uint32_t display_no )
{
  AVPacket packet {};
  packet.buf = nullptr;
  packet.pts = uint64_t( presentation_no ) * uint64_t( MKV_TIMEBASE ) / uint64_t( frame_rate_ );
  packet.dts = uint64_t( display_no ) * uint64_t( MKV_TIMEBASE ) / uint64_t( frame_rate_ );
  packet.data = const_cast<uint8_t*>(
    reinterpret_cast<const uint8_t*>( nal.data() ) ); /* hope that av_write_frame doesn't change contents */
  packet.size = nal.size();
  packet.stream_index = 0;
  packet.duration = MKV_TIMEBASE / frame_rate_;
  packet.pos = -1;

  if ( is_idr( nal ) ) {
    video_extradata_ = nal;
    video_stream_->codecpar->extradata = reinterpret_cast<uint8_t*>( video_extradata_.data() );
    video_stream_->codecpar->extradata_size = video_extradata_.size();
    packet.flags = AV_PKT_FLAG_KEY;
    idr_hit_ = true;
  } else {
    video_stream_->codecpar->extradata = nullptr;
    video_stream_->codecpar->extradata_size = 0;
  }

  if ( idr_hit_ ) {
    av_check( av_write_frame( context_.get(), &packet ) );
    av_check( av_write_frame( context_.get(), nullptr ) );
    avio_flush( context_->pb );
  }
}
