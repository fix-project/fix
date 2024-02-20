#pragma once

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <glog/logging.h>

#include "handle.hh"

/**
 * Serves the purpose of a "blocked queue" in a conventional OS; since we know computations are deterministic, we
 * instead use a directed graph to deduplicate redundant work.
 *
 * This class is not thread-safe, and should be protected by a mutex.
 */
class DependencyGraph
{
public:
  using Task = Handle<Relation>;
  using Result = Handle<Object>;

private:
  absl::flat_hash_set<Task> running_ {};
  absl::flat_hash_map<Task, absl::flat_hash_set<Task>> forward_dependencies_ {};
  absl::flat_hash_map<Task, absl::flat_hash_set<Task>> backward_dependencies_ {};

public:
  DependencyGraph() {}

  bool contains( Task task ) const { return running_.contains( task ); }

  /**
   * Marks a Task as started.  Returns whether the Task is new.
   *
   * @p task  The Task to mark as started.
   * @return  If the Task should actually be started (i.e., if the Task is new).
   */
  bool start( Task task )
  {
    VLOG( 1 ) << "starting " << task;
    if ( contains( task ) )
      return false;
    running_.insert( task );
    return true;
  }

  /**
   * Marks a Task as depending upon another.  Returns whether the dependee is new.
   *
   * @p blocked   The Task to mark as blocked on @p runnable.
   * @p runnable  The Task to mark as runnable, blocking @p blocked.
   * @return      If the @p runnable should actually be started.
   */
  bool add_dependency( Task blocked, Task runnable )
  {
    VLOG( 1 ) << "adding dependency from " << blocked << " to " << runnable;
    forward_dependencies_[blocked].insert( runnable );
    backward_dependencies_[runnable].insert( blocked );
    running_.erase( blocked );
    return start( runnable );
  }

  /**
   * Marks a Task as depending upon another.
   *
   * @p blocked   The Task to mark as blocked on @p runnable.
   * @p runnable  The Task to mark as runnable, blocking @p blocked.
   */
  void add_dependency_no_running( Task blocked, Task runnable )
  {
    VLOG( 1 ) << "adding dependency from " << blocked << " to " << runnable << " without running";
    forward_dependencies_[blocked].insert( runnable );
    backward_dependencies_[runnable].insert( blocked );
    running_.erase( blocked );
  }

  /**
   * Marks a Task as complete, and determines which other Tasks are now ready to be run.
   *
   * @p[in]   task        The Task to mark as complete.
   * @p[out]  unblocked   The set of Tasks which can now be started.
   */
  void finish( Task task, absl::flat_hash_set<Task>& unblocked )
  {
    VLOG( 1 ) << "finished " << task;
    running_.erase( task );
    if ( backward_dependencies_.contains( task ) ) {
      for ( const auto dependent : backward_dependencies_[task] ) {
        auto& target = forward_dependencies_[dependent];
        target.erase( task );
        if ( target.empty() ) {
          VLOG( 2 ) << "resuming " << dependent;
          unblocked.insert( dependent );
          forward_dependencies_.erase( dependent );
        }
      }
      backward_dependencies_.erase( task );
    }
  }
};
