#pragma once

#include "dependency_graph.hh"
#include "interface.hh"
#include "mutex.hh"
#include "relater.hh"

class Scheduler
{
public:
  /*
   * Schedules @p top_level_work given the list of terminal jobs @p leaf_jobs, dependency graph @p graph, and list
   * of workers.It calls `get` on chosen locations for the work.
   *
   * @param remotes        List of remote workers
   * @param local          Local worker
   * @param graph          DependencyGraph containing correct dependency entries for @p top_level_job
   * @param leaf_jobs      List of leaf jobs for this @p top_level_job
   * @param top_level_job  The job to be scheduled
   */
  virtual void schedule( SharedMutex<std::vector<std::weak_ptr<IRuntime>>>& remotes,
                         std::shared_ptr<IRuntime> local,
                         SharedMutex<DependencyGraph>& graph,
                         std::vector<Handle<Relation>>& leaf_jobs,
                         Handle<Relation> top_level_job,
                         Relater& rt )
    = 0;

  virtual ~Scheduler() {};
};

class LocalFirstScheduler : public Scheduler
{
public:
  virtual void schedule( SharedMutex<std::vector<std::weak_ptr<IRuntime>>>& remotes,
                         std::shared_ptr<IRuntime> local,
                         SharedMutex<DependencyGraph>& graph,
                         std::vector<Handle<Relation>>& leaf_jobs,
                         Handle<Relation> top_level_job,
                         Relater& rt ) override;
};

class OnePassScheduler : public Scheduler
{
  std::shared_ptr<IRuntime> schedule_rec( SharedMutex<std::vector<std::weak_ptr<IRuntime>>>& remotes,
                                          std::shared_ptr<IRuntime> local,
                                          SharedMutex<DependencyGraph>& graph,
                                          Handle<Relation> top_level_job,
                                          Relater& rt );

  std::shared_ptr<IRuntime> locate(
    SharedMutex<std::vector<std::weak_ptr<IRuntime>>>& remotes,
    std::shared_ptr<IRuntime> local,
    Handle<Relation> job,
    Relater& rt,
    std::unordered_map<Handle<Relation>, std::shared_ptr<IRuntime>> dependency_locations = {} );

public:
  virtual void schedule( SharedMutex<std::vector<std::weak_ptr<IRuntime>>>& remotes,
                         std::shared_ptr<IRuntime> local,
                         SharedMutex<DependencyGraph>& graph,
                         std::vector<Handle<Relation>>& leaf_jobs,
                         Handle<Relation> top_level_job,
                         Relater& rt ) override;
};
