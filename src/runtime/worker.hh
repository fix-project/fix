#pragma once

#include <boost/lockfree/queue.hpp>
#include <condition_variable>
#include <thread>

#include "job.hh"

#include "elfloader.hh"
#include "wasmcompiler.hh"

class RuntimeStorage;

class RuntimeWorker
{
private:
  friend class RuntimeStorage;

  boost::lockfree::queue<Job> jobs_;

  // TODO can place a map of locally active jobs here

  std::thread thread_;

  size_t thread_id_;

  RuntimeStorage& runtimestorage_;

public:
  RuntimeWorker( size_t thread_id, RuntimeStorage& runtimestorage )
    : jobs_( 0 )
    , thread_()
    , thread_id_( thread_id )
    , runtimestorage_( runtimestorage )
  {
    // The main thread does not need to spawn a thread
    if ( thread_id_ != 0 ) {
      thread_ = std::thread( &RuntimeWorker::work, this );
    }
  }

  void queue_job( Job job );

  bool dequeue_job( Job& job );

  Name force_thunk( Name name );

  Name force_tree( Name name );

  Name force( Name name );

  Name reduce_thunk( Name name );

  Name evaluate_encode( Name encode_name );

  // Force the popped job
  void compute_job( Job& job );

  // Continue computing work if work exists in tasks_ or can be stolen
  void work();
};
