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

        // always send to the zeroth runner
        runners_[0].get().start( std::move( next ) );
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
