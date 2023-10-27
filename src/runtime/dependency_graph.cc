#include "dependency_graph.hh"

#include <glog/logging.h>

std::optional<Handle> DependencyGraph::get( Task task )
{
  std::shared_lock lock( mutex_ );
  return cache_.get( task );
}

void DependencyGraph::check_start( Task&& task )
{
  if ( running_.contains( task ) ) {
    VLOG( 1 ) << task << " is already running.";
    return;
  }
  running_.insert( task );
  VLOG( 1 ) << task << " is being scheduled.";
  scheduler_.start( std::move( task ) );
}

void DependencyGraph::add_result_cache( IResultCache& cache )
{
  caches_.emplace_back( cache );
}

std::optional<Handle> DependencyGraph::start( Task&& task )
{
  std::unique_lock lock( mutex_ );
  VLOG( 1 ) << "Starting " << task;
  auto cached = cache_.get( task );
  if ( cached ) {
    return cached;
  }
  if ( running_.contains( task ) ) {
    return {};
  }
  check_start( std::move( task ) );
  return {};
}

std::optional<Handle> DependencyGraph::start_after( Task& target, Task& blocked )
{
  std::unique_lock lock( mutex_ );
  VLOG( 1 ) << blocked << " is blocked on " << target;
  CHECK( blocked != target );
  auto cached = cache_.get( target );
#ifdef ENABLE_TRACING
  retained_dependencies_[blocked].insert( target );
#endif
  if ( cached ) {
    return cached;
  }
  forward_dependencies_[target].insert( blocked );
  backward_dependencies_[blocked].insert( target );
  check_start( std::move( target ) );
  running_.erase( blocked );
  return {};
}

bool DependencyGraph::start_after( Tree targets, Task& blocked )
{
  VLOG( 1 ) << blocked << " is blocked on filling a " << targets.size() << "-tree.";
  std::unique_lock lock( mutex_ );
  bool found_all = true;
  for ( size_t i = 0; i < targets.size(); i++ ) {
    auto target = Task::Eval( targets[i] );
    if ( not cache_.contains( target ) ) {
      VLOG( 1 ) << blocked << " is blocked on " << target;
      CHECK( blocked != target );
#ifdef ENABLE_TRACING
      retained_dependencies_[blocked].insert( target );
#endif
      forward_dependencies_[target].insert( blocked );
      backward_dependencies_[blocked].insert( target );
      check_start( Task( target ) );
      found_all = false;
    }
  }
  if ( not found_all ) {
    running_.erase( blocked );
  }
  return found_all;
}

void DependencyGraph::finish( Task&& task, Handle handle )
{
  VLOG( 1 ) << task << " is finished with " << handle;
  std::unique_lock lock( mutex_ );
  running_.erase( task );
  cache_.finish( Task( task ), handle );
  for ( const auto& cache : caches_ ) {
    cache.get().finish( Task( task ), handle );
  }
  if ( forward_dependencies_.contains( task ) ) {
    for ( auto unblocked : forward_dependencies_.at( task ) ) {
      auto& deps = backward_dependencies_[unblocked];
      deps.erase( task );
      if ( deps.empty() ) {
        VLOG( 1 ) << unblocked << " is unblocked";
        check_start( Task( unblocked ) );
        backward_dependencies_.erase( unblocked );
      } else {
        VLOG( 1 ) << unblocked << " is waiting on " << deps.size() << " tasks.";
        for ( const auto& d : deps ) {
          VLOG( 2 ) << unblocked << " is waiting on " << d;
        }
      }
    }
    forward_dependencies_.erase( task );
  }
}

Handle DependencyGraph::run( Task&& task )
{
  auto result = start( Task( task ) );
  if ( result )
    return *result;
  VLOG( 1 ) << "Waiting for " << task;
  return cache_.wait( std::move( task ) );
}
