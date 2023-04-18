#pragma once

#include <boost/lockfree/queue.hpp>
#include <condition_variable>
#include <thread>

#include "job.hh"

#include "elfloader.hh"

class RuntimeStorage;

class RuntimeWorker
{
private:
  friend class RuntimeStorage;
  friend class Job;

  boost::lockfree::queue<Job> jobs_;

  std::atomic<bool> liveliness_;

  std::thread thread_;

  size_t thread_id_;

  RuntimeStorage& runtimestorage_;

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

  void force( Name hash, Name name );

  void eval( Name hash, Name name );

  void apply( Name hash, Name name );

  void child( Name hash );

  void link( Name hash, Name name );

  void update_parent( Name name );

  void progress( Name hash, Name name );

  bool resolve_parent( Name hash, Name name );

  // Continue computing work if work exists in tasks_ or can be stolen
  void work();
};
