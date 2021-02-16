#include "sender.hh"
#include "ewma.hh"

using namespace std;

void NetworkSender::summary( ostream& out ) const
{
  out << "Sender info:";

  out << " RTT=";
  Timer::pp_ns( out, stats_.smoothed_rtt );

  if ( stats_.frames_dropped ) {
    out << " frames_dropped=" << stats_.frames_dropped << "!";
  }

  if ( stats_.empty_packets ) {
    out << " empty_packets=" << stats_.empty_packets;
  }

  if ( stats_.bad_acks ) {
    out << " bad_acks=" << stats_.bad_acks << "!";
  }

  if ( stats_.packet_transmissions > 0 ) {
    out << " losses=" << stats_.packet_losses() << "/" << stats_.packet_transmissions << " = " << setprecision( 1 )
        << setw( 2 ) << 100.0 * stats_.packet_losses() / float( stats_.packet_transmissions ) << "%";
  }

  if ( stats_.packet_loss_false_positives ) {
    out << " loss false positives=" << stats_.packet_loss_false_positives << "!";
  }

  if ( stats_.frames_departed_by_expiration ) {
    out << " frames expired=" << stats_.frames_departed_by_expiration << "!";
  }

  if ( stats_.invalid_timestamp ) {
    out << " invalid timestamps=" << stats_.invalid_timestamp << "!";
  }

  if ( greatest_sack_.has_value() ) {
    out << " greatest_sack=" << greatest_sack_.value();
  }

  optional<uint32_t> first_outstanding;

  unsigned int num_outstanding = 0, num_in_flight = 0;
  /*
  const span_view<AudioFrameStatus> statuses
    = frame_status_.region( frame_status_.range_begin(), next_frame_index_ - frame_status_.range_begin() );
  */
  for ( uint32_t i = frame_status_.range_begin(); i < next_frame_index_; i++ ) {
    const auto& status = frame_status_[i];
    if ( not status.outstanding ) {
      continue;
    }

    num_outstanding++;

    if ( status.in_flight ) {
      num_in_flight++;
    }

    if ( not first_outstanding.has_value() ) {
      first_outstanding = i;
    }
  }
  out << " frames in-flight/outstanding=" << num_in_flight << "/" << num_outstanding;

  if ( first_outstanding.has_value() ) {
    out << " out=" << first_outstanding.value() << " - " << next_frame_index_;
  }

  out << " packets_in_flight range = [" << packets_in_flight_.range_begin() << " - " << next_sequence_number_
      << "]";

  out << "\n";
}

void NetworkSender::push_frame( OpusEncoderProcess& encoder )
{
  if ( frames_.range_begin() != frame_status_.range_begin() ) {
    throw runtime_error( "NetworkSender internal error" );
  }

  /*
  if ( encoder.frame_index() != next_frame_index_ ) {
    throw runtime_error( "encoder/sender index mismatch" );
  }
  */

  if ( next_frame_index_ < frames_.range_begin() ) {
    throw runtime_error( "NetworkSender internal error: next_frame_index_ < frames_.range_begin()" );
  }

  if ( need_immediate_send_ ) {
    throw runtime_error( "packet pushed but not sent" );
  }

  if ( next_frame_index_ >= frames_.range_end() ) {
    const size_t frames_to_drop = next_frame_index_ - frames_.range_end() + 1;
    frames_.pop( frames_to_drop );
    frame_status_.pop( frames_to_drop );
    stats_.frames_dropped += frames_to_drop;
  }

  frames_.at( next_frame_index_ )
    = { next_frame_index_, Audio::TwoChannel, encoder.front_enc1(), encoder.front_enc2() };
  frame_status_.at( next_frame_index_ ) = { true, false };
  next_frame_index_++;

  need_immediate_send_ = true;

  encoder.pop_frame();
}

void NetworkSender::set_sender_section( Packet::SenderSection& p )
{
  if ( frames_.range_begin() != frame_status_.range_begin() ) {
    throw runtime_error( "NetworkSender internal error" );
  }

  p.sequence_number = next_sequence_number_++;

  /* send some frames! */
  if ( frames_.range_begin() == next_frame_index_ ) { // nothing to send
    stats_.empty_packets++;
  } else {
    /* always send the most recent frame if it needs it */
    const auto& most_recent_frame = frames_.at( next_frame_index_ - 1 );
    auto& most_recent_status = frame_status_.at( next_frame_index_ - 1 );
    if ( most_recent_status.needs_send() ) {
      p.frames.push_back( most_recent_frame );
      most_recent_status.in_flight = true;
      need_immediate_send_ = false;
    }

    /* now, attempt to fill up the other slots for frames in the packet */
    span<AudioFrameStatus> statuses
      = frame_status_.region( frame_status_.range_begin(), next_frame_index_ - frame_status_.range_begin() );
    const span_view<AudioFrame> frames
      = frames_.region( frame_status_.range_begin(), next_frame_index_ - frame_status_.range_begin() );
    for ( uint32_t i = 0; i < statuses.size(); i++ ) {
      auto& status = statuses[i];

      if ( status.needs_send() ) {
        p.frames.push_back( frames[i] );
        status.in_flight = true;

        if ( p.frames.length >= p.frames.capacity ) {
          break;
        }
      }
    }
  }

  /* make room to store the packet in flight */
  if ( p.sequence_number >= packets_in_flight_.range_end() ) {
    const size_t num_packets_to_drop = p.sequence_number - packets_in_flight_.range_end() + 1;

    const span_view<PacketSentRecord> packets_to_drop
      = packets_in_flight_.region( packets_in_flight_.range_begin(), num_packets_to_drop );
    for ( const auto& pack : packets_to_drop ) {
      assume_departed( pack, false );
    }

    packets_in_flight_.pop( num_packets_to_drop );
  }

  /* record it */
  auto& pack = packets_in_flight_.at( p.sequence_number );
  pack.record = p.to_record();
  pack.assumed_lost = false;
  pack.acked = false;
  pack.sent_timestamp = Timer::timestamp_ns();
  stats_.packet_transmissions++;
}

void NetworkSender::assume_departed( const PacketSentRecord& pack, const bool is_loss )
{
  if ( pack.acked or pack.assumed_lost ) {
    return;
  }

  bool frame_departed = false;
  for ( const uint32_t frame_to_mark : pack.record.frames ) {
    // frame might have been dropped or delivered already
    if ( frame_to_mark >= frame_status_.range_begin() and frame_to_mark < frame_status_.range_end()
         and frame_status_[frame_to_mark].outstanding and frame_status_[frame_to_mark].in_flight ) {
      frame_status_[frame_to_mark].in_flight = false;
      frame_departed = true;
    }
  }

  if ( frame_departed ) {
    if ( is_loss ) {
      stats_.packet_losses_detected++;
    } else {
      stats_.frames_departed_by_expiration++;
    }
  }
}

void NetworkSender::receive_receiver_section( const Packet::ReceiverSection& receiver_section )
{
  if ( frames_.range_begin() != frame_status_.range_begin() ) {
    throw runtime_error( "NetworkSender internal error" );
  }

  if ( receiver_section.next_frame_needed >= frames_.range_end() ) {
    stats_.bad_acks++;
    return;
  }

  if ( receiver_section.next_frame_needed > frames_.range_begin() ) {
    const size_t num_to_pop = receiver_section.next_frame_needed - frames_.range_begin();
    frames_.pop( num_to_pop );
    frame_status_.pop( num_to_pop );
  }

  optional<uint32_t> greatest_new_sack;

  const uint64_t now = Timer::timestamp_ns();

  /* For each selectively ACKed packet, mark its Frames as no longer outstanding */
  for ( const uint32_t sack : receiver_section.packets_received ) {
    if ( sack >= packets_in_flight_.range_end() ) {
      stats_.bad_acks++;
      return;
    }

    if ( greatest_new_sack.has_value() ) {
      greatest_new_sack = max( greatest_new_sack.value(), sack );
    } else {
      greatest_new_sack = sack;
    }

    if ( sack >= packets_in_flight_.range_begin() ) {
      auto& pack = packets_in_flight_.at( sack );
      if ( sack != pack.record.sequence_number ) {
        throw runtime_error( "NetworkSender internal error: sack " + to_string( sack ) + " != pack.seqno "
                             + to_string( pack.record.sequence_number ) );
      }

      if ( pack.acked ) {
        continue;
      }

      if ( pack.assumed_lost ) {
        stats_.packet_loss_false_positives++;
      }

      pack.acked = true;

      const int64_t time_diff = now - pack.sent_timestamp;
      if ( time_diff <= 0 ) {
        stats_.invalid_timestamp++;
      } else {
        ewma_update( stats_.smoothed_rtt, float( time_diff ), stats_.SRTT_ALPHA );
      }

      for ( const uint32_t frame_index : pack.record.frames ) {
        if ( frame_index >= frame_status_.range_end() ) {
          throw runtime_error( "NetworkSender internal error: frame >= frame_status_.range_end()" );
        }

        if ( frame_index >= frame_status_.range_begin() ) {
          frame_status_.at( frame_index ) = { false, false };
        }
      }
    }
  }

  /* For each packet sent "significantly" before the most recent acked packet, assume lost if not delivered */
  if ( not greatest_new_sack.has_value() ) {
    return;
  }

  if ( greatest_sack_.has_value() and greatest_new_sack.value() <= greatest_sack_.value() ) {
    return;
  }

  const uint32_t start_of_range_to_assume_departed = departure_adjudicated_until_seqno();

  uint32_t end_of_range_to_assume_departed;
  if ( greatest_new_sack.value() > reorder_window ) {
    end_of_range_to_assume_departed = greatest_new_sack.value() - reorder_window;
  } else {
    end_of_range_to_assume_departed = packets_in_flight_.range_begin();
  }

  for ( unsigned int seqno = start_of_range_to_assume_departed; seqno < end_of_range_to_assume_departed; seqno++ ) {
    if ( not packets_in_flight_[seqno].acked ) {
      assume_departed( packets_in_flight_[seqno], true );
      packets_in_flight_[seqno].assumed_lost = true;
    }
  }

  greatest_sack_ = greatest_new_sack;

  stats_.last_good_ack_ts = now;
}

uint32_t NetworkSender::departure_adjudicated_until_seqno() const
{
  uint32_t ret;
  if ( greatest_sack_.has_value() and greatest_sack_.value() > reorder_window ) {
    ret = greatest_sack_.value() - reorder_window;
  } else {
    ret = packets_in_flight_.range_begin();
  }
  return ret;
}
