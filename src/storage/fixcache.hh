#pragma once

#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <shared_mutex>
#include <stdexcept>

#include "absl/container/flat_hash_map.h"
#include "absl/hash/hash.h"
#include "entry.hh"
#include "task.hh"

/**
 * The "Fix Cache" contains all the information we know about the relationships between Fix objects, both running
 * and completed.  It can be viewed as a combination of a memoization cache and a dependency graph.
 *
 * The memoization cache is a map from Task to Handle, where the Handle is the result of executing that Task.  The
 * memoization cache can additionally have an entry mapping a Task to std::nullopt, which signals that the Task has
 * already been scheduled but hasn't completed yet.
 *
 * The dependency graph is made up of a multimap from Task to Task, as well as a map from Task to std::size_t.  The
 * former, FixCache.dependency_graph_, associates dependencies with their dependent(s).  Each Task key is associated
 * with an index, allowing one dependency Task to map to many dependent Tasks.  When a running Task completes, all
 * its dependents' blocked counts are updated.  The blocked counts are stored in the second map,
 * FixCache::blocked_count_.  When the blocked count of a Task reaches zero, that Task is "unblocked" and sent back
 * to the scheduler.
 */
class FixCache
{
public:
  /**
   * Rather than tying FixCache to a particular scheduler implementation, we allow the caller to specify their own
   * "schedule" callback.  This callback must be of type `Task -> void`.
   */
  using QueueFunction = std::function<void( Task )>;

private:
  absl::flat_hash_map<Task, std::optional<Handle>, absl::Hash<Task>> fixcache_;

  absl::flat_hash_map<std::pair<Task, std::size_t>, Task, absl::Hash<std::pair<Task, std::size_t>>>
    dependency_graph_;
  absl::flat_hash_map<std::pair<Task, std::size_t>, Task, absl::Hash<std::pair<Task, std::size_t>>>
    inverted_dependency_graph_;
  absl::flat_hash_map<Task, std::shared_ptr<std::atomic<std::size_t>>, absl::Hash<Task>> blocked_count_;
  std::shared_mutex fixcache_mutex_;
  std::condition_variable_any fixcache_cv_;

  /**
   * Marks that execution of @p depender is blocked until the result of @p dependee has been calculated.
   * Used for resuming work.
   *
   * @pre FixCache::fixcache_mutex_ must be locked in exclusive mode.
   *
   * @param dependee  The task which can make progress.
   * @param depender  The task which is blocked.
   */
  void insert_pending_dependency( Task dependee, Task depender )
  {
    if ( dependee == depender ) {
      throw std::runtime_error( "attempted to insert self-dependency" );
    }
    for ( size_t i = 1;; ++i ) {
      // we deal with duplicates by adding a dependency index; we linear scan until we find a free index
      auto pair = std::make_pair( dependee, i );
      auto ins = dependency_graph_.insert( { pair, depender } );
      // If insertion was sucessful
      if ( ins.second ) {
        break;
      }
    }
  }

  /**
   * Marks that completion of @p depender requires @p dependee, regardless of whether @p dependee has been
   * calculated or not.
   *
   * Used specifically for tracing computation progress. The inverted graph is not guaranteed to match the
   * dependency graph.
   *
   * @pre FixCache::fixcache_mutex_ must be locked in exclusive mode.
   *
   * @param dependee  The task which must run first.
   * @param depender  The task which is blocked.
   */
  void insert_reverse_dependency( Task dependee, Task depender )
  {
    for ( size_t i = 1;; ++i ) {
      auto pair = std::make_pair( depender, i );
      if ( !inverted_dependency_graph_.contains( pair ) ) {
        inverted_dependency_graph_.insert( { pair, dependee } );
        break;
      }
      // avoid inserting a duplicate dependency
      if ( inverted_dependency_graph_.at( pair ) == dependee ) {
        break;
      }
    }
  }

  /**
   * Resumes any jobs which were blocked only by @p finished.  Also reduces the blocked count of any other tasks
   * blocked by @p finished.
   *
   * @pre FixCache::fixcache_mutex_ must be locked in at least shared mode.  Additionally, the result of @p finished
   * should be present in FixCache::fixcache_;
   *
   * @param finished  The task which is finished.
   * @param queue     A callback to add a Task to the runnable queue.
   */
  void unblock_jobs( Task finished, QueueFunction queue )
  {
    for ( int i = 1;; ++i ) {
      auto pair = std::make_pair( finished, i );
      if ( dependency_graph_.contains( pair ) ) {
        Task requestor = dependency_graph_.at( pair );
        if ( fixcache_.contains( requestor ) && fixcache_.at( requestor ).has_value() ) {
          throw std::runtime_error( "attempting to reduce blocked count of already finished task" );
        }
        size_t blocked = blocked_count_.at( requestor )->fetch_sub( 1 ) - 1;
        if ( blocked == 0 ) {
          queue( requestor );
        }
      } else {
        break;
      }
    }
  }

  /**
   * Attempts to register a Task as "pending", enqueuing it if possible.  If the task is already running or
   * complete, does nothing.
   *
   * @pre FixCache::fixcache_mutex_ must be locked in exclusive mode.
   *
   * @param task    The task to register.
   * @param queue   A callback to add a Task to the runnable queue.
   * @return        true if the Task was new (and therefore enqueued), false if it already existed.
   */
  bool add_task( Task task, QueueFunction queue )
  {
    if ( not fixcache_.contains( task ) ) {
      fixcache_.insert( { task, std::nullopt } );
      blocked_count_.insert( { task, std::make_shared<std::atomic<std::size_t>>( 0 ) } );
      queue( task );
      return true;
    }
    return false;
  }

public:
  FixCache()
    : fixcache_()
    , dependency_graph_()
    , inverted_dependency_graph_()
    , blocked_count_()
    , fixcache_mutex_()
    , fixcache_cv_()
  {}

  /**
   * Gets the cached result of a Task.
   *
   * @param task  The Task to lookup.
   * @return      If a corresponding entry does not exist in the cache, then std::nullopt is returned. value(std::nullopt) 
   *              is returned if the entry in the cache exists but the result is not cached yet. If the result is 
   *              cached, then value(value(Handle)) of the result is returned.
   */
  std::optional<std::optional<Handle>> get( Task task )
  {
    std::shared_lock lock( fixcache_mutex_ );
    if ( fixcache_.contains( task ) ) {
      return { fixcache_.at( task ) };
    } else {
      return {};
    }
  }

  /**
   * Marks a Task as being completed, caches its result, and resumes any Tasks which are now unblocked as a result.
   * Additionally notifies any thread blocking on the completion of a Task, so it can check if the desired Task is
   * now complete.
   *
   * @pre The Task must have already been added, must be unblocked, and must have been started.  It must not already
   * have a cached result.
   *
   * @param task    The Task which just completed.
   * @param result  The Handle of the result of performing @p task.
   * @param queue   A callback to add a Task to the runnable queue.
   *
   */
  void cache( Task task, Handle result, QueueFunction queue )
  {
    std::unique_lock lock( fixcache_mutex_ );
    if ( not fixcache_.contains( task ) ) {
      throw std::runtime_error( "caching result of task which was never started" );
    }
    if ( fixcache_.at( task ).has_value() ) {
      throw std::runtime_error( "caching result of task which already completed" );
    }
    if ( blocked_count_.at( task )->load() != 0 ) {
      throw std::runtime_error( "caching result of task which is still blocked" );
    }
    fixcache_.at( task ) = result;
    fixcache_cv_.notify_all();
    unblock_jobs( task, queue );
  }

  /**
   * Attempts to mark a Task as running, and enqueues it if it was not already running.
   *
   * @param task    The Task to add.
   * @param queue   A callback to add a Task to the runnable queue.
   * @return        true if the Task was new (and therefore enqueued), false if it already existed.
   */
  bool start( Task task, QueueFunction queue )
  {
    std::unique_lock lock( fixcache_mutex_ );
    return add_task( task, queue );
  }

  /**
   * Either gets the result of a Task if it already finished, or marks a dependency on the result of that Task.  In
   * the latter case, also enqueues the Task if it was not already running.
   *
   * @pre @p depender must have already been added.
   *
   * @param dependee  The Task to lookup, or upon which to mark a dependency.
   * @param depender  The Task making the request, which is blocked on the result of @p dependee.
   * @param queue     A callback to add a Task to the runnable queue.
   * @return          The result if it was already cached, or std::nullopt otherwise.
   */
  std::optional<Handle> get_or_add_dependency( Task dependee, Task depender, QueueFunction queue )
  {
    std::unique_lock lock( fixcache_mutex_ );
    add_task( dependee, queue );
    insert_reverse_dependency( dependee, depender );
    if ( fixcache_.at( dependee ).has_value() ) {
      return fixcache_.at( dependee );
    }
    insert_pending_dependency( dependee, depender );
    blocked_count_.at( depender )->fetch_add( 1 );
    return {};
  }

  /**
   * Increments the blocked count of a Task to mark that it's blocked by an out-of-band dependency.  Can also be
   * used with FixCache::add_dependency_or_decrement_blocking_count to avoid a race condition when adding
   * dependencies on many Tasks at once.
   *
   * @pre The Task must have already been added.
   *
   * @param task    The task which is blocked.
   * @param count   The amount by which to increase the blocked count.
   * @return        The new blocked count after the update.
   */
  std::size_t increment_blocked_count( Task task, std::size_t count = 1 )
  {
    std::shared_lock lock( fixcache_mutex_ );
    return blocked_count_.at( task )->fetch_add( count ) + count;
  }

  /**
   * Adds a dependency from @depender on @dependee if @p dependee has not already finished; if it has finished,
   * decrement the blocking count.  This particular set of operations is necessary to avoid a race condition when
   * adding many dependencies; use it with FixCache::increment_blocked_count.
   *
   * @pre @p depender must have already been added.
   *
   * @param dependee  The Task upon which to mark a dependency.
   * @param depender  The Task making the request, which is blocked on the result of @p dependee.
   * @param queue     A callback to add a Task to the runnable queue.
   * @return          The blocked count of @p depender.
   */
  size_t add_dependency_or_decrement_blocked_count( Task dependee, Task depender, QueueFunction queue )
  {
    std::unique_lock lock( fixcache_mutex_ );
    add_task( dependee, queue );
    insert_reverse_dependency( dependee, depender );
    if ( fixcache_.at( dependee ).has_value() ) {
      return blocked_count_.at( depender )->fetch_sub( 1 ) - 1;
    }
    insert_pending_dependency( dependee, depender );
    return *blocked_count_.at( depender );
  }

  /**
   * Gets the result of a Task if it's cached; otherwise blocks until the Task is finished.
   *
   * @param target  The Task too lookup.
   * @return        The result of @target.
   */
  Handle get_or_block( Task target )
  {
    using namespace std;
    unique_lock lock( fixcache_mutex_ );
    fixcache_cv_.wait(
      lock, [this, target] { return fixcache_.contains( target ) and fixcache_.at( target ).has_value(); } );
    return fixcache_.at( target ).value();
  }

  std::vector<Task> get_dependees( Task depender )
  {
    std::shared_lock lock( fixcache_mutex_ );
    std::vector<Task> result = {};

    for ( size_t i = 1;; ++i ) {
      auto pair = std::make_pair( depender, i );
      if ( !inverted_dependency_graph_.contains( pair ) ) {
        break;
      }
      result.push_back( inverted_dependency_graph_.at( pair ) );
    }

    return result;
  }
};
