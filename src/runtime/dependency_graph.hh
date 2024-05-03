#pragma once

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <glog/logging.h>

#include "handle.hh"
#include "overload.hh"

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
  absl::flat_hash_map<Task, absl::flat_hash_set<Handle<AnyDataType>>> forward_dependencies_ {};
  absl::flat_hash_map<Handle<AnyDataType>, absl::flat_hash_set<Task>> backward_dependencies_ {};

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
   * Marks a Task as depending upon another.
   *
   * @p blocked   The Task to mark as blocked on @p runnable.
   * @p runnable_or_loadable  The Task to mark as runnable or The Handle<Fix> to be loaded, blocking @p blocked
   */
  void add_dependency( Task blocked, Handle<AnyDataType> runnable_or_loadable )
  {
    VLOG( 1 ) << "adding dependency from " << blocked << " to " << runnable_or_loadable << " without running";
    forward_dependencies_[blocked].insert( runnable_or_loadable );
    backward_dependencies_[runnable_or_loadable].insert( blocked );
    running_.erase( blocked );
  }

  /**
   * Marks a Task as complete, and determines which other Tasks are now ready to be run.
   *
   * @p[in]   task_or_object        The Task to mark as complete or The Object loaded.
   * @p[out]  unblocked   The set of Tasks which can now be started.
   */
  void finish( Handle<AnyDataType> task_or_object, absl::flat_hash_set<Task>& unblocked )
  {
    VLOG( 1 ) << "finished " << task_or_object;
    task_or_object.visit<void>( overload { [&]( Handle<Relation> r ) { running_.erase( r ); }, [&]( auto ) {} } );
    if ( backward_dependencies_.contains( task_or_object ) ) {
      for ( const auto dependent : backward_dependencies_[task_or_object] ) {
        auto& target = forward_dependencies_[dependent];
        target.erase( task_or_object );
        if ( target.empty() ) {
          VLOG( 2 ) << "resuming " << dependent;
          unblocked.insert( dependent );
          forward_dependencies_.erase( dependent );
        }
      }
      backward_dependencies_.erase( task_or_object );
    }
  }

  absl::flat_hash_set<Handle<AnyDataType>> get_forward_dependencies( Task blocked ) const
  {
    if ( forward_dependencies_.contains( blocked ) ) {
      return forward_dependencies_.at( blocked );
    } else {
      return {};
    }
  }
};
