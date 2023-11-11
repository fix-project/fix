#pragma once

#include "dependency_graph.hh"
#include "interface.hh"
#include "runtimestorage.hh"
#include "wasm-rt.h"

class Runtime;

class WorkerPool : public ITaskRunner
{
  size_t num_workers_;
  Channel<Task> runq_;

  std::vector<std::unique_ptr<RuntimeWorker>> workers_ {};

public:
  WorkerPool( size_t num_workers )
    : num_workers_( num_workers )
    , runq_()
  {
    wasm_rt_init();
  }

  void start( Runtime& runtime, DependencyGraph& graph, RuntimeStorage& storage )
  {
    for ( size_t i = 0; i < num_workers_; ++i ) {
      workers_.push_back( std::make_unique<RuntimeWorker>( i, runtime, graph, storage, runq_ ) );
      workers_.back()->start();
    }
  }

  void stop() { workers_.clear(); }

  std::optional<Info> get_info() override { return Info { .parallelism = static_cast<uint32_t>( num_workers_ ) }; }

  std::optional<Handle> start( Task&& task ) override
  {
    runq_.push( std::move( task ) );
    return {};
  }
};
