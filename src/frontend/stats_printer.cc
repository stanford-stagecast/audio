#include "stats_printer.hh"

#include <cxxabi.h>

using namespace std;
using namespace std::chrono;

StatsPrinterTask::StatsPrinterTask( std::shared_ptr<EventLoop> loop )
  : loop_( loop )
  , standard_output_( CheckSystemCall( "dup STDERR_FILENO", dup( STDERR_FILENO ) ) )
  , next_stats_print( steady_clock::now() )
  , next_stats_reset( steady_clock::now() )
{
  loop_->add_rule(
    "generate+print statistics",
    [&] {
      ss_.str( {} );
      ss_.clear();

      for ( const auto& obj : objects_ ) {
        if ( obj ) {
          obj->summary( ss_ );
          obj->reset_summary();
        }
      }

      loop_->summary( ss_ );
      ss_ << "\n";
      global_timer().summary( ss_ );
      ss_ << "\n";

      /* calculate new time */
      const auto now = steady_clock::now();
      next_stats_print = now + stats_print_interval;

      /* reset if necessary */
      if ( now > next_stats_reset ) {
        loop_->reset_summary();

        next_stats_reset = now + stats_reset_interval;
      }

      /* print out */
      const auto& str = ss_.str();
      if ( output_rb_.writable_region().size() >= str.size() ) {
        output_rb_.push_from_const_str( str );
      }
      output_rb_.pop_to_fd( standard_output_ );
    },
    [&] { return steady_clock::now() > next_stats_print; } );

  loop_->add_rule(
    "print statistics",
    standard_output_,
    Direction::Out,
    [&] { output_rb_.pop_to_fd( standard_output_ ); },
    [&] { return output_rb_.bytes_stored() > 0; } );
}

unsigned int StatsPrinterTask::wait_time_ms() const
{
  const auto now = steady_clock::now();
  if ( now > next_stats_print ) {
    return 0;
  } else {
    return duration_cast<milliseconds>( next_stats_print - now ).count();
  }
}
