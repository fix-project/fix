#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <stdexcept>

#include <glog/logging.h>

#include "absl/container/flat_hash_map.h"
#include "absl/hash/hash.h"
#include "handle.hh"
#include "operation.hh"
#include "relation.hh"
#include "task.hh"

#include "interface.hh"

/**
 * The primary storage of every fact we discover about a computation.  At least for now, "fact" is defined very
 * narrowly: this cache only stores mappings from Tasks to their results.  For correctness, any "fact" must have the
 * property that it is deterministic and time-invariant; once a fact has been stored in the cache, it will never
 * change.  This cache is effectively append-only; it is impossible to externally remove a result once it has been
 * saved; this may change in the future with the addition of garbage collection, but even so a result should still
 * semantically be immutable.
 */
class FixCache : IResultCache
{
  std::shared_mutex mutex_ {};
  std::condition_variable_any cv_ {};
  absl::flat_hash_map<Task, Handle, absl::Hash<Task>> cache_ {};
  std::unordered_set<Task> finished_ {};

public:
  /**
   * Store the fact that @p task resulted in @p handle.  If this is inconsistent with a previously-known fact, throw
   * an exception.
   *
   * @param task   The Task whose execution completed.
   * @param handle The result of @p task.
   */
  void finish( Task&& task, Handle handle ) override
  {
    std::unique_lock lock( mutex_ );
    if ( cache_.contains( task ) ) {
      LOG( WARNING ) << "Warning: task " << task << " was finished more than once.\n";
      if ( cache_[task] != handle ) {
        throw std::runtime_error( "Task was finished multiple times with different results!\n" );
      }
    }
    cache_.insert( std::make_pair( task, handle ) );
    finished_.insert( task );
    cv_.notify_all();
  }

  /**
   * Try to get the saved result of a Task.  Does not perform any work if the result is unknown.
   *
   * @param task  The task for which to get a result.
   * @return      The result, if known, otherwise `std::nullopt`.
   */
  std::optional<Handle> get( Task task )
  {
    std::shared_lock lock( mutex_ );
    if ( cache_.contains( task ) )
      return cache_.at( task );
    return {};
  }

  std::optional<Relation> get_relation( Task task )
  {
    std::shared_lock lock( mutex_ );
    if ( cache_.contains( task ) )
      return Relation( task, cache_.at( task ) );

    if ( task.operation() == Operation::Eval && task.handle().is_blob() ) {
      return Relation( task, task.handle() );
    }

    return {};
  }

  /**
   * Tests if FixCache has already seen the result of running a task.  Semantically equivalent to
   * `get(task).has_value()`.
   *
   * @param task  The Task in question.
   * @return      Whether or not the result is already stored in FixCache.
   */
  bool contains( Task task ) { return get( task ).has_value(); }

  /**
   * Block until the result of @p task is known.  If it's already known, returns immediately; if not, puts the
   * thread to sleep until another thread receives the result.
   *
   * @param task  The Task in question.
   * @return      The Handle of the result.
   */
  Handle wait( Task&& task )
  {
    std::shared_lock lock( mutex_ );
    cv_.wait( lock, [&] { return cache_.contains( task ); } );
    return cache_.at( task );
  }
};
