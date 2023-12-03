#pragma once

#include <condition_variable>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"

#include "fixcache.hh"
#include "object.hh"
#include "scheduler.hh"

#ifndef ENABLE_TRACING
#define ENABLE_TRACING 0
#endif

/**
 * Serves the purpose of a "blocked queue" in a conventional OS; since we know computations are deterministic, we
 * instead use a directed graph to deduplicate redundant work.
 */
class DependencyGraph
  : public ITaskRunner
  , public IResultCache
{
  std::shared_mutex mutex_ {};
  std::condition_variable_any cv_ {};
  FixCache& cache_;
  Scheduler& scheduler_;
  std::vector<std::reference_wrapper<IResultCache>> caches_ {};
  absl::flat_hash_set<Task> running_ {};
  absl::flat_hash_map<Task, absl::flat_hash_set<Task>> forward_dependencies_ {};
  absl::flat_hash_map<Task, absl::flat_hash_set<Task>> backward_dependencies_ {};
#if ENABLE_TRACING == 1
  absl::flat_hash_map<Task, absl::flat_hash_set<Task>> retained_dependencies_ {};
#endif

  /**
   * Starts a Task, @p task, if and only if it has not already been started.
   */
  void check_start( Task&& task );

public:
  DependencyGraph( FixCache& cache, Scheduler& scheduler )
    : cache_( cache )
    , scheduler_( scheduler )
  {
    (void)scheduler_;
  }

  /**
   * Adds a result cache to be notified whenever a new result is learned.
   *
   * @param cache   A result cache; this reference must remain valid until DependencyGraph is destroyed.
   */
  void add_result_cache( IResultCache& cache );

  /**
   * Queries this graph's associated FixCache for the result of a Task.
   *
   * @param task  The Task to look up.
   * @return      The result, if it's already known.
   */
  std::optional<Handle> get( Task task );

  /**
   * Returns a copy of the forward dependency graph, from tasks to tasks which depend on them.
   * Returns an empty map if ENABLE_TRACING is 0.
   *
   * @return   A map from tasks to a set of tasks which depend on the key.
   */
  absl::flat_hash_map<Task, absl::flat_hash_set<Task>> get_graph()
  {
#if ENABLE_TRACING == 1
    std::shared_lock lock( mutex_ );
    return retained_dependencies_;
#else
    return {};
#endif
  }

  /**
   * Starts @p task if its result is not already known.
   *
   * @param task  The Task to look up or start.
   * @return      The result, if it's already known.
   */
  std::optional<Handle> start( Task&& task ) override;

  /**
   * Starts @p blocked after the result of @p target is known, starting @p target if necessary.  If the result of @p
   * target is already known, immediately returns it and doesn't start anything.
   *
   * @param target  The Task to look up or start.
   * @param blocked The Task to resume afterwards.
   * @return        The result, if it's already known.
   */
  std::optional<Handle> start_after( Task& target, Task& blocked );

  /**
   * Starts @p blocked after the results of evaluating every entry in @p targets is known.  Starts any evaluations
   * which are not already running or complete. If the results are all already known, returns true and doesn't start
   * anything.
   *
   * @param targets A Tree of Handles to look up or start evaluation of.
   * @param blocked The Task to resume afterwards.
   * @return        True if all the targets are already evaluated.
   */
  bool start_after( Tree targets, Task& blocked );

  /**
   * Mark that @p task has been finished with the result @p handle, and resume any other Tasks which were blocked on
   * this result.
   *
   * @param task    The finished Task.
   * @param handle  The result.
   */
  void finish( Task&& task, Handle handle ) override;

  /**
   * Start @p task and wait for it to complete.
   *
   * @param task  The Task to start.
   * @return      The result of @p task.
   */
  Handle run( Task&& task );
};
