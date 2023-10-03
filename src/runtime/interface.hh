#pragma once

#include "task.hh"

/**
 * A worker which is capable of running a Task, whether locally or remotely.
 */
class ITaskRunner
{
public:
  /**
   * Metadata describing the environment in which this ITaskRunner executes Tasks.  This information is provided to
   * the scheduler in order to make decisions about where to assign different Tasks.
   */
  struct Info
  {
    uint32_t parallelism;
  };

  /**
   * Assigns a Task @p task to this ITaskRunner.  If the ITaskRunner already knows the result of this Task through
   * any means (e.g., a memoization cache), it may return the result immediately.  If it returns `std::nullopt`,
   * this ITaskRunner will guarantee that this Task gets executed.
   *
   * @param task  The Task to execute.
   * @return      The result of the Task, if it's already known.
   */
  virtual std::optional<Handle> start( Task&& task ) = 0;

  /**
   * Gets metadata about this ITaskRunner.
   *
   * @return The worker's information, if known.
   */
  virtual std::optional<Info> get_info() = 0;
  virtual ~ITaskRunner() {};
};

/**
 * An entity potentially interested in the result or completion of a Task.
 */
class IResultCache
{
public:
  /**
   * Notify this IResultCache that running @p task generated @p result.
   *
   * @param task    The Task which was completed.
   * @param result  The Handle of the Task's result.
   */
  virtual void finish( Task&& task, Handle result ) = 0;
  virtual ~IResultCache() {};
};

/**
 * An abstraction over a pool of ITaskRunner and a pool of IResultCache.  When the IRuntime is asked to run a task
 * via IRuntime::start, it will delegate that task to one of its ITaskRunner children.  When it is informed of a
 * result via IRuntime::finish, it will notify all its IResultCache children of that result.
 *
 * It is guaranteed that the result of a Task submitted via IRuntime::start (in the case where it returns
 * `std::nullopt`) will, eventually, be sent to every added IResultCache.  However, there is no guarantee about how,
 * where, or when execution of the Task will happen.
 *
 * All provided references must remain valid until the IRuntime is destroyed.
 */
class IRuntime
  : public ITaskRunner
  , public IResultCache
{
public:
  /**
   * Register an ITaskRunner as a part of this IRuntime's runner pool.  This will make it visible to the
   * IRuntime's scheduling system.
   *
   * @param runner  A worker that can run Tasks.
   */
  virtual void add_task_runner( ITaskRunner& runner ) = 0;

  /**
   * Register an IResultCache as part of this IRuntime's notification receipient pool.  This cache will be notified,
   * via IResultCache::finish, whenever this IRuntime learns of a new result via IRuntime::finish.
   *
   * @param cache   A cache to be notified whenever a new result is discovered.
   */
  virtual void add_result_cache( IResultCache& cache ) = 0;
  virtual ~IRuntime() {};
};
