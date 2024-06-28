#pragma once

#include "evaluator.hh"
#include "pass.hh"
#include "relater.hh"

#include <functional>

class Scheduler : public FixRuntime
{
protected:
  FixEvaluator evaluator_;
  std::optional<std::reference_wrapper<Relater>> relater_ {};

public:
  /*
   * Schedules @p top_level_work given the list of terminal jobs @p leaf_jobs, dependency graph @p graph, and list
   * of workers.It calls `get` on chosen locations for the work.
   *
   * @param top_level_job  The job to be scheduled
   */
  virtual Result<Object> schedule( Handle<Relation> top_level_job ) = 0;

  virtual void set_relater( std::reference_wrapper<Relater> relater ) { relater_ = relater; }

  Scheduler()
    : evaluator_( *this )
  {}

  virtual ~Scheduler() {};
};

class LocalScheduler : public Scheduler
{
public:
  LocalScheduler() {}

  virtual Result<Fix> load( Handle<AnyDataType> value ) override;
  virtual Result<AnyTree> load( Handle<AnyTreeRef> value ) override;
  virtual Handle<AnyTreeRef> ref( Handle<AnyTree> tree ) override;
  virtual Result<Object> apply( Handle<ObjectTree> combination ) override;
  virtual Result<Value> evalStrict( Handle<Object> expression ) override;
  virtual Result<Object> evalShallow( Handle<Object> expression ) override;
  virtual Result<ValueTree> mapEval( Handle<ObjectTree> tree ) override;
  virtual Result<ObjectTree> mapReduce( Handle<ExpressionTree> tree ) override;
  virtual Result<ValueTree> mapLift( Handle<ValueTree> tree ) override;

  virtual Result<Object> schedule( Handle<Relation> top_level_job ) override;
};

inline thread_local std::vector<Handle<AnyDataType>> works_;
inline thread_local std::optional<Handle<Relation>> current_schedule_step_;
inline thread_local bool nested_;

class RelaterTest;

class SketchGraphScheduler : public Scheduler
{
  friend class RelaterTest;

private:
  std::vector<PassRunner::PassType> passes_;
  virtual void run_passes( std::vector<Handle<AnyDataType>>& leaf_jobs, Handle<Relation> top_level_job );

protected:
  void relate( Handle<Relation> top_level_job );

public:
  SketchGraphScheduler( std::vector<PassRunner::PassType>&& passes )
    : Scheduler()
    , passes_( move( passes ) )
  {}

  virtual Result<Fix> load( Handle<AnyDataType> value ) override;
  virtual Result<AnyTree> load( Handle<AnyTreeRef> value ) override;
  virtual Handle<AnyTreeRef> ref( Handle<AnyTree> tree ) override;
  virtual Result<Object> apply( Handle<ObjectTree> combination ) override;
  virtual Result<Value> evalStrict( Handle<Object> expression ) override;
  virtual Result<Object> evalShallow( Handle<Object> expression ) override;
  virtual Result<ValueTree> mapEval( Handle<ObjectTree> tree ) override;
  virtual Result<ObjectTree> mapReduce( Handle<ExpressionTree> tree ) override;
  virtual Result<ValueTree> mapLift( Handle<ValueTree> tree ) override;

  virtual Result<Object> schedule( Handle<Relation> top_level_job ) override;

  void merge_sketch_graph( Handle<Relation> job, absl::flat_hash_set<Handle<Relation>>& unblocked );
  void merge_all_sketch_graph( Handle<Relation> job, absl::flat_hash_set<Handle<Relation>>& unblocked );
};

class OnePassScheduler : public SketchGraphScheduler
{
public:
  OnePassScheduler()
    : SketchGraphScheduler( { PassRunner::PassType::MinAbsentMaxParallelism } )
  {}
};

class HintScheduler : public SketchGraphScheduler
{
public:
  HintScheduler()
    : SketchGraphScheduler( { PassRunner::PassType::MinAbsentMaxParallelism,
                              PassRunner::PassType::ChildBackProp,
                              PassRunner::PassType::InOutSource } )
  {}
};

class RandomScheduler : public SketchGraphScheduler
{
public:
  RandomScheduler()
    : SketchGraphScheduler( { PassRunner::PassType::Random } )
  {}
};
