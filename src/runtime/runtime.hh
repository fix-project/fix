#pragma once

#include "dependency_graph.hh"
#include "interface.hh"
#include "network.hh"
#include "runtimestorage.hh"
#include "scheduler.hh"
#include "worker_pool.hh"

#ifndef RUNTIME_THREADS
#define RUNTIME_THREADS std::thread::hardware_concurrency()
#endif

class Runtime : IRuntime
{
  FixCache cache_;
  RuntimeStorage storage_;
  std::shared_ptr<WorkerPool> workers_;
  Scheduler scheduler_;
  DependencyGraph graph_;
  NetworkWorker network_;

  static inline thread_local Handle current_procedure_;

public:
  Runtime()
    : cache_()
    , storage_()
    , workers_( std::make_shared<WorkerPool>( RUNTIME_THREADS, *this, graph_, storage_ ) )
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
  void finish( Task&& task, Handle result ) override
  {
    auto local_handle = storage_.get_local_name( task.handle() );
    if ( local_handle.has_value() ) {
      Task local_task( local_handle.value(), task.operation() );
      graph_.finish( std::move( local_task ), result );
    }

    graph_.finish( std::move( task ), result );
  }

  std::optional<Info> get_info() override { return workers_->get_info(); }

  void add_task_runner( std::weak_ptr<ITaskRunner> runner ) override { scheduler_.add_task_runner( runner ); }
  void add_result_cache( IResultCache& cache ) override { graph_.add_result_cache( cache ); }

  Handle eval( Handle target ) { return graph_.run( Task::Eval( target ) ); }

  void set_current_procedure( Handle handle ) { current_procedure_ = handle; }

  Handle get_current_procedure() { return current_procedure_; }

  RuntimeStorage& storage() { return storage_; }

  Address start_server( const Address& address ) { return network_.start_server( address ); }

  void connect( const Address& address ) { network_.connect( address ); }
};
