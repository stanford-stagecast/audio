#include "video_source.hh"
#include "timer.hh"

using namespace std;

static constexpr uint64_t frame_interval = 40'000'000; /* almost 1/24 s */

void VideoSource::push( const H264Encoder::EncodedNAL& nal, const uint64_t now )
{
  if ( next_nal_index_ == 0 ) {
    beginning_time_ = Timer::timestamp_ns();
  }

  const string nal_as_string { reinterpret_cast<const char*>( nal.NAL.data() ), nal.NAL.size() };
  outbound_queue_.push( { next_nal_index_++, now + frame_interval, 0, move( nal_as_string ) } );

  if ( not timestamp_next_chunk_.has_value() ) {
    timestamp_next_chunk_.emplace( now );
  }
}

unsigned int VideoSource::TimedNAL::num_chunks() const
{
  static constexpr size_t capacity = VideoChunk::Buffer::capacity();
  const unsigned int result = nal.size() / capacity;
  const unsigned int remainder = nal.size() % capacity;
  return result + ( remainder ? 1 : 0 );
}

size_t VideoSource::TimedNAL::next_chunk_size() const
{
  if ( offset > nal.size() ) {
    throw runtime_error( "next_chunk_size(): internal error" );
  }

  return min( nal.size() - offset, size_t( VideoChunk::Buffer::capacity() ) );
}

string_view VideoSource::TimedNAL::next_chunk() const
{
  string_view ret { nal };
  ret = ret.substr( offset, next_chunk_size() );
  if ( ret.size() != next_chunk_size() ) {
    throw runtime_error( "next_chunk(): internal_error" );
  }
  return ret;
}

bool VideoSource::TimedNAL::last_chunk() const
{
  return offset + next_chunk_size() == nal.size();
}

bool VideoSource::has_frame() const
{
  return not outbound_queue_.empty();
}

bool VideoSource::ready( const uint64_t now ) const
{
  return has_frame() and ( now >= timestamp_next_chunk_ );
}

void VideoSource::pop_frame()
{
  TimedNAL& nal = outbound_queue_.front();
  nal.offset += nal.next_chunk_size();

  if ( nal.offset == nal.nal.size() ) {
    outbound_queue_.pop();
  }

  if ( outbound_queue_.empty() ) {
    timestamp_next_chunk_.reset();
  } else {
    timestamp_next_chunk_.value()
      = min( outbound_queue_.front().timestamp_completion,
             timestamp_next_chunk_.value() + frame_interval / outbound_queue_.front().num_chunks() );
  }
}

uint64_t VideoSource::wait_time_ms( const uint64_t now ) const
{
  if ( ready( now ) ) {
    return 0;
  }

  if ( not has_frame() ) {
    return 60'000;
  }

  return ( timestamp_next_chunk_.value() - now ) / 1'000'000;
}

VideoChunk VideoSource::front( const uint32_t frame_index ) const
{
  VideoChunk ret;
  ret.frame_index = frame_index;
  ret.nal_index = outbound_queue_.front().nal_index;

  ret.data.resize( outbound_queue_.front().next_chunk_size() );
  ret.data.mutable_buffer().copy( outbound_queue_.front().next_chunk() );

  ret.end_of_nal = outbound_queue_.front().last_chunk();

  return ret;
}

void VideoSource::summary( ostream& out ) const
{
  out << "next NAL: " << next_nal_index_;
  out << " fps: " << next_nal_index_ / ( double( Timer::timestamp_ns() - beginning_time_ ) / 1000000000.0 );
  out << "\n";
}

#include "connection.cc"
#include "receiver.cc"
#include "sender.cc"

template class NetworkConnection<VideoChunk, VideoSource>;
template class NetworkSender<VideoChunk>;
template class NetworkReceiver<VideoChunk>;
