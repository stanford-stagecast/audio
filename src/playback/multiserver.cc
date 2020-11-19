#include "multiserver.hh"

#include <chrono>
#include <iostream>

using namespace std;
using namespace std::chrono;

NetworkMultiServer::NetworkMultiServer( EventLoop& loop )
  : socket_()
  , next_cursor_sample_( Timer::timestamp_ns() + cursor_sample_interval )
{
  socket_.set_blocking( false );
  socket_.bind( { "0", 0 } );

  cout << "Port " << socket_.local_address().port() << " " << receive_key_.printable_key().as_string_view() << " "
       << send_key_.printable_key().as_string_view() << endl;

  loop.add_rule( "network receive", socket_, Direction::In, [&] { receive_packet(); } );

  loop.add_rule(
    "sample cursors",
    [&] {
      const uint64_t now = Timer::timestamp_ns();
      next_cursor_sample_ = now + cursor_sample_interval;

      /* get mean sample index */
      uint64_t sample_index_sum {}, sample_index_num {};
      for ( auto& cl : clients_ ) {
        if ( cl.has_value() and cl.value().cursor.sample_index().has_value() ) {
          sample_index_num++;
          sample_index_sum += cl.value().cursor.sample_index().value();
        }
      }

      if ( sample_index_num ) {
        global_sample_index_ = sample_index_sum / sample_index_num;
      }

      /* sample each client */
      for ( auto& cl : clients_ ) {
        if ( cl.has_value() ) {
          cl->cursor.sample( cl->endpoint, now, global_sample_index_ );
        }
      }

      /* discard old decoded audio */
      for ( auto& cl : clients_ ) {
        if ( cl.has_value() ) {
          if ( global_sample_index_ > cl->cursor.output().ch1().range_begin() + 1000 ) {
            cl->cursor.output().pop( global_sample_index_ - cl->cursor.output().ch1().range_begin() - 1000 );
          }
        }
      }
    },
    [&] { return Timer::timestamp_ns() >= next_cursor_sample_; } );

  loop.add_rule(
    "mix and encode audio",
    [&] {
      for ( auto& cl : clients_ ) {
        if ( cl.has_value() ) {
          mix_and_encode( *cl );
          cl->endpoint.send_packet( crypto_, cl->addr, socket_ );
        }
      }
      next_encode_index_ += 120;
    },
    [&] { return global_sample_index_ >= next_encode_index_; } );
}

void NetworkMultiServer::receive_packet()
{
  Address src { nullptr, 0 };
  Ciphertext ciphertext;
  socket_.recv( src, ciphertext.data );

  /* decrypt */
  Plaintext plaintext;
  if ( not crypto_.decrypt( ciphertext, plaintext ) ) {
    stats_.decryption_failures++;
    return;
  }

  /* parse */
  Parser parser { plaintext };
  const Packet packet { parser };
  if ( parser.error() ) {
    stats_.invalid++;
    return;
  }

  /* create client? */
  if ( not clients_.at( packet.node_id ).has_value() ) {
    clients_.at( packet.node_id ).emplace( packet.node_id, src );
  }

  Client& client = clients_.at( packet.node_id ).value();

  client.endpoint.act_on_packet( packet );
  /* throw away frames no longer needed */
  if ( client.cursor.next_frame_index() > client.endpoint.frames().range_begin() ) {
    client.endpoint.pop_frames( client.cursor.next_frame_index() - client.endpoint.frames().range_begin() );
  }
}

NetworkMultiServer::Client::Client( const uint8_t s_node_id, const Address& s_addr )
  : node_id( s_node_id )
  , addr( s_addr )
  , cursor( 20, true )
{
  for ( auto& x : gains ) {
    x.first = x.second = 1.0;
  }
}

void NetworkMultiServer::summary( ostream& out ) const
{
  out << "Network multiserver:";

  if ( stats_.decryption_failures ) {
    out << " decryption_failures=" << stats_.decryption_failures << "!";
  }

  if ( stats_.invalid ) {
    out << " invalid=" << stats_.invalid << "!";
  }

  out << "\n";

  for ( const auto& client : clients_ ) {
    if ( client.has_value() ) {
      client->summary( out );
    }
  }
}

void NetworkMultiServer::Client::summary( ostream& out ) const
{
  out << "   #" << int( node_id ) << "(" << addr.to_string() << "):";
  cursor.summary( out );
  out << "\n";
  endpoint.summary( out );
}

void NetworkMultiServer::mix_and_encode( Client& client )
{
  /* prepare mixed audio */
  span<float> ch1 = client.mixed_ch1.region( global_sample_index_, 120 );
  span<float> ch2 = client.mixed_ch2.region( global_sample_index_, 120 );

  fill( ch1.begin(), ch1.end(), 0.0 );
  fill( ch2.begin(), ch2.end(), 0.0 );

  for ( uint8_t channel_i = 0; channel_i < client.gains.size(); channel_i += 2 ) {
    const optional<Client>& cl = clients_.at( channel_i / 2 );
    if ( cl.has_value() ) {
      /* mix in other channels */

      const span_view<float>& other_ch1 = cl->cursor.output().ch1().region( global_sample_index_, 120 );
      const span_view<float>& other_ch2 = cl->cursor.output().ch2().region( global_sample_index_, 120 );

      for ( size_t sample_i = 0; sample_i < 120; sample_i++ ) {
        ch1[sample_i] += client.gains[channel_i].first * other_ch1[sample_i];
        ch2[sample_i] += client.gains[channel_i].second * other_ch1[sample_i];

        ch1[sample_i] += client.gains[channel_i + 1].first * other_ch2[sample_i];
        ch2[sample_i] += client.gains[channel_i + 1].second * other_ch2[sample_i];
      }
    }
  }

  /* encode */
  client.encoder_.encode_one_frame( client.mixed_ch1, client.mixed_ch2 );
  client.mixed_ch1.pop( 120 );
  client.mixed_ch2.pop( 120 );

  client.endpoint.push_frame( client.encoder_ );
}
