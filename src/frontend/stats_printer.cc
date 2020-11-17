#include "stats_printer.hh"

using namespace std;
using namespace std::chrono;

StatsPrinterTask::StatsPrinterTask( const shared_ptr<AudioDeviceTask> device,
                                    const shared_ptr<NetworkEndpoint> network,
                                    const shared_ptr<EventLoop> loop )
  : device_( device )
  , network_( network )
  , loop_( loop )
  , standard_output_( CheckSystemCall( "dup STDOUT_FILENO", dup( STDOUT_FILENO ) ) )
  , output_rb_( 65536 )
  , next_stats_print( steady_clock::now() + stats_print_interval )
  , next_stats_reset( steady_clock::now() + stats_reset_interval )
{
  loop_->add_rule(
    "generate+print statistics",
    [&] {
      ss_.str( {} );
      ss_.clear();

      if ( device_ ) {
        device_->generate_statistics( ss_ );
      }
      network_->generate_statistics( ss_ );
      ss_ << "\n";
      loop_->summary( ss_ );
      ss_ << "\n";

      const auto now = steady_clock::now();
      next_stats_print = now + stats_print_interval;
      if ( now > next_stats_reset ) {
        loop_->reset_statistics();
        next_stats_reset = now + stats_reset_interval;
      }

      global_timer().summary( ss_ );
      ss_ << "\n";

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
