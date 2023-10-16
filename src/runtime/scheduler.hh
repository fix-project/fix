#pragma once

#include "channel.hh"
#include "interface.hh"

class Scheduler : public ITaskRunner
{
  Channel<Task> to_be_scheduled_ {};
  std::vector<std::reference_wrapper<ITaskRunner>> runners_;
  std::thread scheduler_thread_;

  void schedule()
  {
    try {
      while ( true ) {
        Task next = to_be_scheduled_.pop_or_wait();

        // always send to the runner with the most cores
        size_t destination = 0;
        size_t ncores = 0;
        for ( size_t i = 0; i < runners_.size(); i++ ) {
          uint32_t cores
            = runners_[i].get().get_info().value_or( ITaskRunner::Info { .parallelism = 0 } ).parallelism;
          if ( runners_.size() != 1 )
            std::cerr << "Node " << i << " has " << cores << " cores.\n";
          if ( cores > ncores ) {
            destination = i;
            ncores = cores;
          }
        }
        if ( runners_.size() != 1 )
          std::cerr << "Scheduling " << next << " on " << destination << "\n";
        runners_[destination].get().start( std::move( next ) );
      }
    } catch ( ChannelClosed& ) {
      return;
    }
  }

public:
  Scheduler( ITaskRunner& runner )
    : runners_ { runner }
    , scheduler_thread_( std::bind( &Scheduler::schedule, this ) )
  {}

  void add_task_runner( ITaskRunner& runner ) { runners_.emplace_back( runner ); }

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
