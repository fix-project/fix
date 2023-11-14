#include <glog/logging.h>

using namespace std;

#include "scheduler.hh"

void Scheduler::schedule()
{
  try {
    while ( true ) {
      Task next = to_be_scheduled_.pop_or_wait();
      VLOG( 1 ) << "Scheduling " << next;

      // always send to the runner with the most cores
      std::shared_ptr<ITaskRunner> destination;
      size_t ncores = 0;

      size_t i = 0;
      size_t selected = 0;

      std::unique_lock lock( runners_mutex_ );
      for ( auto it = runners_.begin(); it != runners_.end(); ) {
        if ( auto p = it->lock() ) {
          uint32_t cores = p->get_info().value_or( ITaskRunner::Info { .parallelism = 0 } ).parallelism;
          VLOG( 2 ) << "Node " << i << " has " << cores << " cores.\n";
          if ( cores > ncores ) {
            selected = i;
            destination = p;
            ncores = cores;
          }
          it++;
        } else {
          it = runners_.erase( it );
        }
        i++;
      }

      if ( destination ) {
        VLOG( 2 ) << "Scheduling " << next << " on " << selected << "\n";
        destination->start( std::move( next ) );
      } else {
        throw std::runtime_error( "No available runner" );
      }
    }
  } catch ( ChannelClosed& ) {
    return;
  }
}
