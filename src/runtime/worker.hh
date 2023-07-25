#pragma once

#include <boost/lockfree/queue.hpp>
#include <condition_variable>
#include <queue>
#include <thread>

#include "task.hh"

#include "elfloader.hh"

class RuntimeStorage;

class RuntimeWorker
{
private:
  friend class RuntimeStorage;
  friend class Job;

  /// A queue of runnable Tasks.
  boost::lockfree::queue<Task> runq_;

  /// The thread corresponding to this Worker.
  std::thread thread_;

  /// The thread's index in RuntimeStorage.
  size_t thread_id_;

  /// A reference to the parent RuntimeStorage corresponding to this thread's pool.
  RuntimeStorage& runtimestorage_;

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
   * The callback to take ownership of a Task, which in this case adds it to the scheduleable queue of
   * RuntimeStorage.
   *
   * @param task  The inbound Task, which should be added to the runnable queue.
   */
  std::function<void( Task )> queue_cb;

public:
  RuntimeWorker( size_t thread_id, RuntimeStorage& runtimestorage )
    : runq_( 0 )
    , thread_()
    , thread_id_( thread_id )
    , runtimestorage_( runtimestorage )
    , queue_cb( std::bind( &RuntimeWorker::queue_job, this, std::placeholders::_1 ) )
  {
    thread_ = std::thread( &RuntimeWorker::work, this );
  }

  /**
   * Adds a Task to this worker's "run queue", which effectively transfers ownership of the Task to this Worker.
   *
   * @param task  The Task to schedule.
   */
  void queue_job( Task task );

  /**
   * Attempts to get a runnable Task from *somewhere*; that somewhere may be this worker or another worker.  The
   * caller takes ownership of the Task.
   *
   * @return The Task if any was found, or else std::nullopt.
   */
  std::optional<Task> dequeue_job();

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

  /**
   * Continuously execute all available tasks until RuntimeStorage::threads_active_ is set to false.
   */
  void work();
};
