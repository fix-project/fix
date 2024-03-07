#pragma once

#include "interface.hh"
#include "relater.hh"
#include <functional>

class Scheduler
{
protected:
  std::optional<std::reference_wrapper<Relater>> relater_ {};

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
  virtual void schedule( std::vector<Handle<Relation>>& leaf_jobs, Handle<Relation> top_level_job ) = 0;

  virtual void set_relater( std::reference_wrapper<Relater> relater ) { relater_ = relater; }

  virtual ~Scheduler() {};
};

class LocalFirstScheduler : public Scheduler
{
public:
  virtual void schedule( std::vector<Handle<Relation>>& leaf_jobs, Handle<Relation> top_level_job ) override;
};

class OnePassScheduler : public Scheduler
{
  std::shared_ptr<IRuntime> schedule_rec( Handle<Relation> top_level_job );

  std::shared_ptr<IRuntime> locate(
    Handle<Relation> job,
    const std::unordered_map<Handle<Relation>, std::shared_ptr<IRuntime>>& dependency_locations );

public:
  virtual void schedule( std::vector<Handle<Relation>>& leaf_jobs, Handle<Relation> top_level_job ) override;
};
