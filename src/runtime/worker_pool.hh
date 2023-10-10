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
  WorkerPool( size_t num_workers, Runtime& runtime, DependencyGraph& graph, RuntimeStorage& storage )
    : num_workers_( num_workers )
    , runq_()
  {
    wasm_rt_init();

    for ( size_t i = 0; i < num_workers_; ++i ) {
      workers_.push_back( std::make_unique<RuntimeWorker>( i, runtime, graph, storage, runq_ ) );
    }
  }

  std::optional<Handle> start( Task&& task )
  {
    runq_.push( std::move( task ) );
    return {};
  }
};
