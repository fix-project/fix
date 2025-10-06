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
private:
  Result<Object> select_single( Handle<Object>, size_t );
  Result<Object> select_range( Handle<Object>, size_t begin_idx, size_t end_idx );

public:
  LocalScheduler() {}

  virtual Result<Blob> load( Handle<Blob> value ) override;
  virtual Result<AnyTree> load( Handle<AnyTree> value ) override;
  virtual Result<AnyTree> load( Handle<AnyTreeRef> value ) override;
  virtual Result<AnyTree> loadShallow( Handle<AnyTree> ) override;
  virtual Handle<AnyTreeRef> ref( Handle<AnyTree> tree ) override;
  virtual Result<Object> select( Handle<ObjectTree> ) override;
  virtual Result<Object> apply( Handle<ObjectTree> combination ) override;
  virtual Result<Value> evalStrict( Handle<Object> expression ) override;
  virtual Result<Object> force( Handle<Thunk> thunk ) override;
  virtual Result<ValueTree> mapEval( Handle<ObjectTree> tree ) override;
  virtual Result<ObjectTree> mapReduce( Handle<ExpressionTree> tree ) override;
  virtual Result<ValueTree> mapLift( Handle<ValueTree> tree ) override;
  virtual Result<ObjectTree> mapEvalShallow( Handle<ObjectTree> ) override;

  virtual Result<Object> schedule( Handle<Relation> top_level_job ) override;
};

inline thread_local std::optional<Handle<Relation>> current_schedule_step_;
inline thread_local bool nested_;
inline thread_local bool go_for_it_;

class RelaterTest;

class SketchGraphScheduler : public Scheduler
{
  friend class RelaterTest;

protected:
  std::vector<PassRunner::PassType> passes_;
  virtual Result<Object> run_passes( Handle<Relation> top_level_job );
  bool loadShallow( Handle<AnyTree>, Handle<AnyTreeRef> );
  Result<Object> select_single( Handle<Object>, size_t );
  Result<Object> select_range( Handle<Object>, size_t begin_idx, size_t end_idx );

protected:
  void relate( Handle<Relation> top_level_job );

public:
  SketchGraphScheduler( std::vector<PassRunner::PassType>&& passes )
    : Scheduler()
    , passes_( move( passes ) )
  {}

  virtual Result<Blob> load( Handle<Blob> value ) override;
  virtual Result<AnyTree> load( Handle<AnyTree> value ) override;
  virtual Result<AnyTree> load( Handle<AnyTreeRef> value ) override;
  virtual Result<AnyTree> loadShallow( Handle<AnyTree> ) override;
  virtual Handle<AnyTreeRef> ref( Handle<AnyTree> tree ) override;
  virtual Result<Object> select( Handle<ObjectTree> ) override;
  virtual Result<Object> apply( Handle<ObjectTree> combination ) override;
  virtual Result<Value> evalStrict( Handle<Object> expression ) override;
  virtual Result<Object> force( Handle<Thunk> thunk ) override;
  virtual Result<ValueTree> mapEval( Handle<ObjectTree> tree ) override;
  virtual Result<ObjectTree> mapReduce( Handle<ExpressionTree> tree ) override;
  virtual Result<ValueTree> mapLift( Handle<ValueTree> tree ) override;
  virtual Result<ObjectTree> mapEvalShallow( Handle<ObjectTree> ) override;

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
