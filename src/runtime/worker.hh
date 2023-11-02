#pragma once

#include <condition_variable>
#include <functional>
#include <thread>

#include "channel.hh"
#include "dependency_graph.hh"
#include "task.hh"

class Runtime;
class RuntimeStorage;

class RuntimeWorker
{
private:
  /// A queue of runnable Tasks.
  Channel<Task>& runq_;

  /// The thread corresponding to this Worker.
  std::thread thread_;

  /// The thread's index in RuntimeStorage.
  size_t thread_id_;

  /// This thread's thread id in RuntimeStorage (on any thread, this will be the same as thread_id_).
  static inline thread_local size_t current_thread_id_;

  Runtime& runtime_;
  DependencyGraph& graph_;
  RuntimeStorage& storage_;

  /**
   * Either gets the result of a subtask, or marks a dependency and transfers ownership of the current task to
   * FixCache.
   *
   * @param target    The subtask from which we need a result.
   * @param current   The currently running Task.
   * @return          The result of the subtask, if computed already, else std::nullopt.
   */
  std::optional<Handle> await( Task target, Task current );

  /**
   * Marks dependencies on the evaluation of all the children of the tree specified by @p task.  If any children
   * have yet to be evaluated (e.g., if the return code is nonzero), transfers ownership of @p task to FixCache.
   *
   * @param task  A Task corresponding to the evaluation of a tree-like object.
   * @return      true if the dependencies are all already evaluated, else false.
   */
  bool await_tree( Task task );

  /**
   * Marks a task as having finished with a specific result.  Transfers ownership of the finished Task back to
   * FixCache, and claims ownership of any unblocked Tasks.
   *
   * @param task    The finished Task.
   * @param result  The handle of the result of @p task.
   */
  void cache( Task task, Handle result );

  /**
   * Continuously execute all available tasks until RuntimeStorage::threads_active_ is set to false.
   */
  void work();

  /**
   * Attempts the "evaluate" operation specified by a Task.  Takes ownership of the Task.
   *
   * @param task  The Eval task.
   * @return      The result of the evaluation if execution completed, otherwise std::nullopt.
   */
  std::optional<Handle> do_eval( Task task );

  /**
   * Attempts the "apply" operation specified by a Task.  Takes ownership of the Task.
   *
   * @param task  The Apply task.
   * @return      The result of the application.
   */
  Handle do_apply( Task task );

  /**
   * Attempts the "fill" pseudo-operation on a Handle to a Tree or a Tag.
   *
   * @param handle  The Handle of the tree-like to fill.
   * @return        The result of the filling.
   */
  Handle do_fill( Handle handle );

  /**
   * Executes a Task, or schedules it for future execution.
   *
   * @param task  The task to execute.
   * @return      The result of execution, if it completed; otherwise std::nullopt.
   */
  std::optional<Handle> progress( Task task );

  void setup();

public:
  RuntimeWorker( size_t thread_id,
                 Runtime& runtime,
                 DependencyGraph& graph,
                 RuntimeStorage& storage,
                 Channel<Task>& runq )
    : runq_( runq )
    , thread_( std::bind( &RuntimeWorker::work, this ) )
    , thread_id_( thread_id )
    , runtime_( runtime )
    , graph_( graph )
    , storage_( storage )
  {}

  ~RuntimeWorker()
  {
    runq_.close();
    thread_.join();
  }
};
