#include "multiserver.hh"

#include <chrono>
#include <iostream>

using namespace std;
using namespace std::chrono;

uint64_t NetworkMultiServer::server_clock() const
{
  return ( Timer::timestamp_ns() - global_ns_timestamp_at_creation_ ) * 48 / 1000000;
}

void NetworkMultiServer::receive_keyrequest( const Address& src, const Ciphertext& ciphertext )
{
  /* decrypt */
  for ( auto& client : clients_ ) {
    if ( client.try_keyrequest( src, ciphertext, socket_ ) ) {
      return;
    }
  }
  stats_.bad_packets++;
}

void NetworkMultiServer::add_key( const LongLivedKey& key )
{
  const uint8_t next_id = clients_.size() + 1;
  const uint8_t ch1 = 2 * clients_.size();
  const uint8_t ch2 = ch1 + 1;
  clients_.emplace_back( next_id, ch1, ch2, key );
  cerr << "Added key #" << int( next_id ) << " for: " << key.name() << " on channels " << int( ch1 ) << ":"
       << int( ch2 ) << "\n";

  internal_board_.set_channel_name( ch1, string( key.name() ) );
  internal_board_.set_channel_name( ch2, string( key.name() ) + "-CH2" );

  program_board_.set_channel_name( ch1, string( key.name() ) );
  program_board_.set_channel_name( ch2, string( key.name() ) + "-CH2" );
}

void NetworkMultiServer::initialize_clock()
{
  next_cursor_sample_ = server_clock() + opus_frame::NUM_SAMPLES;
}

NetworkMultiServer::NetworkMultiServer( const uint8_t num_clients, EventLoop& loop )
  : socket_()
  , global_ns_timestamp_at_creation_( Timer::timestamp_ns() )
  , next_cursor_sample_( server_clock() + opus_frame::NUM_SAMPLES )
  , num_clients_( num_clients )
  , internal_board_( "internal", 2 * num_clients )
  , program_board_( "program", 2 * num_clients )
{
  socket_.set_blocking( false );
  socket_.bind( { "0", 9101 } );

  loop.add_rule( "network receive", socket_, Direction::In, [&] {
    Address src { nullptr, 0 };
    Ciphertext ciphertext;
    ciphertext.resize( socket_.recv( src, ciphertext.mutable_buffer() ) );
    if ( ciphertext.length() > 24 ) {
      const uint8_t node_id = ciphertext.as_string_view().back();
      if ( node_id == uint8_t( KeyMessage::keyreq_id ) ) {
        receive_keyrequest( src, ciphertext );
      } else if ( node_id > 0 and node_id <= clients_.size() ) {
        clients_.at( node_id - 1 ).receive_packet( src, ciphertext, server_clock() );
      } else {
        stats_.bad_packets++;
      }
    } else {
      stats_.bad_packets++;
    }
  } );

  loop.add_rule(
    "mix+encode+send",
    [&] {
      const uint64_t ts_now = Timer::timestamp_ns();

      /* decode all audio */
      for ( auto& client : clients_ ) {
        if ( client ) {
          client.client().decode_audio( next_cursor_sample_, internal_board_, program_board_ );
          if ( client.client().connection().sender_stats().last_good_ack_ts + CLIENT_TIMEOUT_NS < ts_now ) {
            client.clear_current_session();
          }
        }
      }

      /* mix all audio */
      for ( auto& client : clients_ ) {
        if ( client ) {
          client.client().mix_and_encode( internal_board_, next_cursor_sample_ );
        }
      }

      internal_audio_.mix_and_write( internal_board_, next_cursor_sample_ );
      program_audio_.mix_and_write( program_board_, next_cursor_sample_ );

      /* send audio to clients */
      for ( auto& client : clients_ ) {
        if ( client ) {
          client.client().send_packet( socket_ );
        }
      }

      if ( next_cursor_sample_ > 960 ) {
        internal_board_.pop_samples_until( next_cursor_sample_ - 960 );
        program_board_.pop_samples_until( next_cursor_sample_ - 960 );
      }

      next_cursor_sample_ += opus_frame::NUM_SAMPLES;
    },
    [&] { return server_clock() >= next_cursor_sample_; } );
}

void NetworkMultiServer::summary( ostream& out ) const
{
  out << "bad packets: " << stats_.bad_packets << "\n";
  for ( const auto& client : clients_ ) {
    if ( client ) {
      out << "#" << int( client.client().peer_id() ) << ": ";
      client.summary( out );
    }
  }
}

void NetworkMultiServer::json_summary( Json::Value& root, const bool include_second_channels ) const
{
  internal_board_.json_summary( root["board"][internal_board_.name()], include_second_channels );
  program_board_.json_summary( root["board"][program_board_.name()], include_second_channels );

  for ( const auto& client : clients_ ) {
    if ( client ) {
      client.client().json_summary( root["client"][client.name()] );
    } else {
      Client::default_json_summary( root["client"][client.name()] );
    }
  }
}

void NetworkMultiServer::set_cursor_lag( const string_view name,
                                         const string_view feed,
                                         const uint16_t target_samples,
                                         const uint16_t min_samples,
                                         const uint16_t max_samples )
{
  for ( auto& client : clients_ ) {
    if ( client and client.name() == name ) {
      client.client().set_cursor_lag( feed, target_samples, min_samples, max_samples );
    }
  }
}

void NetworkMultiServer::set_gain( const string_view board_name,
                                   const string_view channel_name,
                                   const float gain1,
                                   const float gain2 )
{
  AudioBoard* target = nullptr;
  if ( internal_board_.name() == board_name ) {
    target = &internal_board_;
  } else if ( program_board_.name() == board_name ) {
    target = &program_board_;
  }

  if ( target ) {
    target->set_gain( channel_name, gain1, gain2 );
  }
}
