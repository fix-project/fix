#pragma once

#include "dependency_graph.hh"
#include "interface.hh"
#include "network.hh"
#include "runtimestorage.hh"
#include "scheduler.hh"
#include "worker_pool.hh"

class Runtime : IRuntime
{
  FixCache cache_;
  RuntimeStorage storage_;
  WorkerPool workers_;
  Scheduler scheduler_;
  DependencyGraph graph_;
  NetworkWorker network_;

  static inline thread_local Handle current_procedure_;

public:
  Runtime()
    : cache_()
    , storage_()
    , workers_( std::thread::hardware_concurrency(), *this, graph_, storage_ )
    , scheduler_( workers_ )
    , graph_( cache_, scheduler_ )
    , network_( *this )
  {
    graph_.add_result_cache( network_ );
  }

  static Runtime& get_instance()
  {
    static Runtime runtime;
    return runtime;
  }

  std::optional<Handle> start( Task&& task ) override { return graph_.start( std::move( task ) ); }
  void finish( Task&& task, Handle result ) override { graph_.finish( std::move( task ), result ); }

  void add_task_runner( ITaskRunner& runner ) override { scheduler_.add_task_runner( runner ); }
  void add_result_cache( IResultCache& cache ) override { graph_.add_result_cache( cache ); }

  Handle eval( Handle target ) { return graph_.run( Task::Eval( target ) ); }

  void set_current_procedure( Handle handle ) { current_procedure_ = handle; }

  Handle get_current_procedure() { return current_procedure_; }

  RuntimeStorage& storage() { return storage_; }

  Address start_server( const Address& address ) { return network_.start_server( address ); }

  void connect( const Address& address ) { network_.connect( address ); }
};
