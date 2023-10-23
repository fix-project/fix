#pragma once

#include "channel.hh"
#include "interface.hh"

class Scheduler : public ITaskRunner
{
  Channel<Task> to_be_scheduled_ {};
  std::list<std::weak_ptr<ITaskRunner>> runners_;
  std::thread scheduler_thread_;

  void schedule();

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
