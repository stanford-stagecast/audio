#pragma once

#include <algorithm>
#include <limits>

#include "typed_ring_buffer.hh"

class JitterBuffer : public SafeEndlessBuffer<bool>
{
public:
  struct Stat
  {
    static constexpr float ALPHA = 1.0 / 50.0;

    int offset;
    float readability { 1.0 };

    Stat( const size_t s_offset )
      : offset( s_offset )
    {}

    void accumulate( const bool can_read ) { readability = ALPHA * can_read + ( 1 - ALPHA ) * readability; }
  };

private:
  std::vector<Stat> stats_ {};

public:
  using SafeEndlessBuffer<bool>::SafeEndlessBuffer;

  void add_sample_point( const size_t playback_offset ) { stats_.emplace_back( playback_offset ); }

  void sample( const size_t playback_index, const int playback_offset )
  {
    for ( auto& stat : stats_ ) {
      /* is audio playable with the given offset? */
      const int64_t adjusted_playback_index = playback_index - playback_offset + stat.offset;

      if ( adjusted_playback_index < 0 or adjusted_playback_index > std::numeric_limits<int64_t>::max() ) {
        stat.accumulate( 0 );
        continue;
      }

      const size_t unsigned_adjusted_playback_index = adjusted_playback_index;
      if ( unsigned_adjusted_playback_index < range_begin() or unsigned_adjusted_playback_index > range_end() ) {
        stat.accumulate( 0 );
      } else {
        stat.accumulate( safe_get( unsigned_adjusted_playback_index ) );
      }
    }
  }

  const std::vector<Stat> stats() const { return stats_; }

  std::optional<int> best_offset( const float min_readability ) const
  {
    std::optional<int> best_offset;

    for ( const Stat& stat : stats_ ) {
      if ( stat.readability > min_readability ) {
        if ( best_offset.has_value() ) {
          if ( stat.offset > best_offset.value() ) {
            best_offset = stat.offset;
          }
        } else {
          best_offset = stat.offset;
        }
      }
    }

    return best_offset;
  }
};
