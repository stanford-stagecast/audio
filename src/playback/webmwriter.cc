#include "webmwriter.hh"

#include <iostream>

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

WebMWriter::WebMWriter( const string& output_filename,
                        const int bit_rate,
                        const uint32_t sample_rate,
                        const uint8_t num_channels )
  : audio_stream_()
  , header_written_( false )
  , sample_rate_( sample_rate )
{
  if ( output_filename.substr( output_filename.size() - 5 ) != ".webm" ) {
    throw runtime_error( "output filename must be a .webm" );
  }

  {
    AVFormatContext* tmp_context;
    av_check( avformat_alloc_output_context2( &tmp_context, nullptr, nullptr, output_filename.c_str() ) );
    context_.reset( tmp_context );
  }

  /* open output file */
  av_check( avio_open( &context_->pb, output_filename.c_str(), AVIO_FLAG_WRITE ) );

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
}

WebMWriter::~WebMWriter()
{
  try {
    if ( header_written_ ) {
      av_check( av_write_trailer( context_.get() ) );
    }

    if ( context_->pb ) {
      av_check( avio_close( context_->pb ) );
    }
  } catch ( const exception& e ) {
    cerr << "Exception in WebMWriter destructor: " << e.what() << "\n";
  }
}

void WebMWriter::write( opus_frame& frame, const unsigned int starting_sample_number )
{
  AVPacket packet {};
  packet.buf = nullptr;
  packet.pts = WEBM_TIMEBASE * starting_sample_number / sample_rate_;
  packet.dts = WEBM_TIMEBASE * starting_sample_number / sample_rate_;
  packet.data = frame.mutable_unsigned_data_ptr();
  packet.size = frame.length();
  packet.stream_index = 0;
  packet.flags = AV_PKT_FLAG_KEY;
  packet.duration = WEBM_TIMEBASE * opus_frame::NUM_SAMPLES / sample_rate_;
  packet.pos = -1;

  av_check( av_write_frame( context_.get(), &packet ) );
}
