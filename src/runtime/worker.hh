#pragma once

#include <boost/lockfree/queue.hpp>
#include <condition_variable>
#include <queue>
#include <thread>

#include "task.hh"

#include "elfloader.hh"

class RuntimeStorage;

class RuntimeWorker
{
private:
  friend class RuntimeStorage;
  friend class Job;

  boost::lockfree::queue<Task> jobs_;

  std::atomic<bool> liveliness_;

  std::thread thread_;

  size_t thread_id_;

  RuntimeStorage& runtimestorage_;

  std::optional<Handle> await( Task target, Task current );
  bool await_tree( Task task );

  void cache( Task task, Handle result );

  std::function<void( Task )> queue_cb;

public:
  RuntimeWorker( size_t thread_id, RuntimeStorage& runtimestorage )
    : jobs_( 0 )
    , liveliness_( true )
    , thread_()
    , thread_id_( thread_id )
    , runtimestorage_( runtimestorage )
    , queue_cb( std::bind( &RuntimeWorker::queue_job, this, std::placeholders::_1 ) )
  {
    thread_ = std::thread( &RuntimeWorker::work, this );
  }

  void queue_job( Task task );

  std::optional<Task> dequeue_job();
  std::optional<Handle> do_eval( Task name );
  Handle do_apply( Task task );
  Handle do_fill( Handle handle );

  std::optional<Handle> progress( Task task );

  // Continue computing work if work exists in tasks_ or can be stolen
  void work();
};
