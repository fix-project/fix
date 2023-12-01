#pragma once

#include "dependency_graph.hh"
#include "interface.hh"
#include "network.hh"
#include "relation.hh"
#include "runtimestorage.hh"
#include "scheduler.hh"
#include "worker_pool.hh"

class Runtime : IRuntime
{
  FixCache cache_;
  RuntimeStorage storage_;
  std::shared_ptr<WorkerPool> workers_;
  Scheduler scheduler_;
  DependencyGraph graph_;
  NetworkWorker network_;

  static inline thread_local Handle current_procedure_;

  void deserialize_relations();

public:
  Runtime( size_t runtime_threads )
    : cache_()
    , storage_()
    , workers_( std::make_shared<WorkerPool>( runtime_threads ) )
    , scheduler_( workers_ )
    , graph_( cache_, scheduler_ )
    , network_( *this )
  {
    graph_.add_result_cache( network_ );
    network_.start();
    workers_->start( *this, graph_, storage_ );
  }

  static Runtime& get_instance( size_t runtime_threads = std::thread::hardware_concurrency() )
  {
    static Runtime runtime( runtime_threads );
    return runtime;
  }

  std::optional<Handle> start( Task&& task ) override { return graph_.start( std::move( task ) ); }

  void finish( Task&& task, Handle result ) override
  {
    auto local_list = storage_.get_local_tasks( task );
    while ( !local_list.empty() ) {
      graph_.finish( std::move( local_list.front() ), result );
      local_list.pop_front();
    }

    graph_.finish( std::move( task ), result );
  }

  std::optional<Info> get_info() override { return workers_->get_info(); }

  void add_task_runner( std::weak_ptr<ITaskRunner> runner ) override { scheduler_.add_task_runner( runner ); }
  void add_result_cache( IResultCache& cache ) override { graph_.add_result_cache( cache ); }

  Handle eval( Handle target ) { return graph_.run( Task::Eval( target ) ); }

  std::optional<Relation> get_relation( Task task ) { return cache_.get_relation( task ); }
  std::list<Relation> get_relation( Handle handle ) { return cache_.get_relation( handle ); }
  std::list<Relation> get_parents( Handle handle ) { return cache_.get_parents( handle ); }

  void set_current_procedure( Handle handle ) { current_procedure_ = handle; }

  Handle get_current_procedure() { return current_procedure_; }

  RuntimeStorage& storage() { return storage_; }

  Address start_server( const Address& address ) { return network_.start_server( address ); }

  void connect( const Address& address ) { network_.connect( address ); }

  ~Runtime()
  {
    workers_->stop();
    network_.stop();
  }

  void visit( Relation root, std::function<void( Relation )> visitor );
  void shallow_visit( Relation root, std::function<void( Relation )> visitor );

  void serialize( Task task );
  void serialize( Handle name );

  void deserialize()
  {
    storage_.deserialize();
    deserialize_relations();
  };

  void trace( Handle obj, Handle trc ) { cache_.trace( obj, trc ); }
};
