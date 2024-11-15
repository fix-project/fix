#pragma once

#include "couponcollector.hh"
#include "runtime.hh"
#include "tagger.hh"

#include <deque>

struct SchedulerContinuation
{
  std::deque<Handle<Value>> loads {};
  std::deque<Handle<Tree>> applys {};

  Handle<Fix> top_level_job {};
  std::deque<std::optional<CouponCollectorTag>> continuation {};
};

class Scheduler
{
protected:
  NonDeterministicRuntime& rt_;
  CouponCollector collector_;
  Tagger& tagger_;

public:
  Scheduler( NonDeterministicRuntime& rt, Tagger& tagger )
    : rt_( rt )
    , collector_( rt )
    , tagger_( tagger )
  {}

  virtual EvalTag schedule( Handle<Fix> top_level_,
                            std::deque<std::optional<CouponCollectorTag>> = {},
                            std::deque<KernelExecutionTag> = {} )
    = 0;
  virtual ~Scheduler() {};
};

class LocalScheduler : public Scheduler
{
private:
  std::deque<std::optional<CouponCollectorTag>> continuation_ {};
  std::deque<KernelExecutionTag> apply_results_ {};

  std::deque<std::optional<CouponCollectorTag>> new_continuation_ {};
  std::deque<Handle<Tree>> to_apply_ {};

  std::optional<Handle<Blob>> load( Handle<Blob> blob );
  std::optional<Handle<Blob>> load( Handle<BlobRef> blob );
  std::optional<Handle<Treeish>> load( Handle<Treeish> tree );
  std::optional<Handle<Treeish>> load( Handle<TreeishRef> tref );
  // virtual Result<Tree> loadShallow( Handle<Tree> ) override;
  Handle<TreeishRef> ref( Handle<Treeish> tree );

  // virtual Result<Object> select( Handle<ObjectTree> ) override;
  std::optional<ApplyTag> apply( Handle<Tree> combination );

  std::optional<ThinkTag> force( Handle<Thunk> thunk );
  std::optional<ThinkTag> force_until_not_thunk( Handle<Thunk> thunk );

  std::optional<ReduceTag> reduce_shallow( Handle<Thunk> expression );
  std::pair<std::optional<ThinkTag>, std::optional<EvalTag>> thunk_strict( Handle<Thunk> expression );
  std::optional<ReduceTag> reduce_strict( Handle<Thunk> expression );
  std::optional<EvalTag> eval( Handle<Fix> expression );
  std::optional<EvalTag> map_eval( Handle<Treeish> tree );

  std::optional<ReduceTag> reduce( Handle<Fix> );
  std::optional<ReduceTag> map_reduce( Handle<Treeish> tree );
  // virtual Result<ValueTree> mapLift( Handle<ValueTree> tree ) override;
  // virtual Result<ObjectTree> mapEvalShallow( Handle<ObjectTree> ) override;

public:
  LocalScheduler( NonDeterministicRuntime& rt, Tagger& tagger )
    : Scheduler( rt, tagger )
  {}

  virtual EvalTag schedule( Handle<Fix> top_level_job,
                            std::deque<std::optional<CouponCollectorTag>> continuation = {},
                            std::deque<KernelExecutionTag> results = {} ) override;
};
