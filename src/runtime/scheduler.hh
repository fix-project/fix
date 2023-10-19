#pragma once

#include "channel.hh"
#include "interface.hh"

class Scheduler : public ITaskRunner
{
  Channel<Task> to_be_scheduled_ {};
  std::list<std::weak_ptr<ITaskRunner>> runners_;
  std::thread scheduler_thread_;

  void schedule()
  {
    try {
      while ( true ) {
        Task next = to_be_scheduled_.pop_or_wait();

        // always send to the runner with the most cores
        std::shared_ptr<ITaskRunner> destination;
        size_t ncores = 0;

        for ( auto it = runners_.begin(); it != runners_.end(); ) {
          if ( auto p = it->lock() ) {
            uint32_t cores = p->get_info().value_or( ITaskRunner::Info { .parallelism = 0 } ).parallelism;
            if ( runners_.size() != 1 )
              std::cerr << "Node " << p << " has " << cores << " cores.\n";
            if ( cores > ncores ) {
              destination = p;
              ncores = cores;
            }
            it++;
          } else {
            it = runners_.erase( it );
          }
        }

        if ( destination ) {
          std::cerr << "Scheduling " << next << " on " << destination << "\n";
          destination->start( std::move( next ) );
        } else {
          throw std::runtime_error( "No available runner" );
        }
      }
    } catch ( ChannelClosed& ) {
      return;
    }
  }

public:
  Scheduler( std::weak_ptr<ITaskRunner> runner )
    : runners_ { runner }
    , scheduler_thread_( std::bind( &Scheduler::schedule, this ) )
  {}

  void add_task_runner( std::weak_ptr<ITaskRunner> runner ) { runners_.emplace_back( runner ); }

  std::optional<Handle> start( Task&& task ) override
  {
    to_be_scheduled_.push( std::move( task ) );
    return {};
  }

  ~Scheduler()
  {
    to_be_scheduled_.close();
    scheduler_thread_.join();
  }
};
