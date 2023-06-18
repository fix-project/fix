#pragma once

#include <boost/lockfree/queue.hpp>
#include <condition_variable>
#include <queue>
#include <thread>

#include "job.hh"

#include "elfloader.hh"

class RuntimeStorage;

class RuntimeWorker
{
private:
  friend class RuntimeStorage;
  friend struct Job;

  boost::lockfree::queue<Job> jobs_;

  std::atomic<bool> liveliness_;

  std::thread thread_;

  size_t thread_id_;

  RuntimeStorage& runtimestorage_;

  void launch_jobs( std::queue<std::pair<Handle, Handle>> ready_jobs );

public:
  RuntimeWorker( size_t thread_id, RuntimeStorage& runtimestorage )
    : jobs_( 0 )
    , liveliness_( true )
    , thread_()
    , thread_id_( thread_id )
    , runtimestorage_( runtimestorage )
  {
    thread_ = std::thread( &RuntimeWorker::work, this );
  }

  void queue_job( Job job );

  bool dequeue_job( Job& job );

  void eval( Handle hash, Handle name );

  void apply( Handle hash, Handle name );

  void fill( Handle hash, Handle name );

  void link( Handle hash, Handle name );

  void update_parent( Handle name );

  void progress( Handle hash, Handle name );

  bool resolve_parent( Handle hash, Handle name );

  // Continue computing work if work exists in tasks_ or can be stolen
  void work();
};
