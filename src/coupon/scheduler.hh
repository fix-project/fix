#pragma once

#include "couponcollector.hh"
#include "runtime.hh"
#include "tagger.hh"

class Scheduler
{
protected:
  NonDeterministicRuntime& rt_;
  CouponCollector collector_;
  Tagger tagger_;

public:
  Scheduler( Runtime& rt )
    : rt_( rt )
    , collector_( (DeterministicTagRuntime&)( rt ) )
    , tagger_( rt )
  {}

  virtual EvalTag schedule( Handle<Fix> top_level_job ) = 0;
  virtual ~Scheduler() {};
};

class LocalScheduler : public Scheduler
{
private:
  Handle<Blob> load( Handle<Blob> blob );
  Handle<Treeish> load( Handle<Treeish> tree );
  Handle<Treeish> load( Handle<TreeishRef> tref );
  // virtual Result<Tree> loadShallow( Handle<Tree> ) override;
  Handle<TreeishRef> ref( Handle<Treeish> tree );

  // virtual Result<Object> select( Handle<ObjectTree> ) override;
  ApplyTag apply( Handle<Tree> combination );

  ThinkTag force( Handle<Thunk> thunk );
  ThinkTag force_until_not_thunk( Handle<Thunk> thunk );

  ReduceTag reduce_shallow( Handle<Thunk> expression );
  ReduceTag reduce_strict( Handle<Thunk> expression );
  EvalTag eval( Handle<Fix> expression );
  EvalTag map_eval( Handle<Treeish> tree );

  ReduceTag reduce( Handle<Fix> );
  ReduceTag map_reduce( Handle<Treeish> tree );
  // virtual Result<ValueTree> mapLift( Handle<ValueTree> tree ) override;
  // virtual Result<ObjectTree> mapEvalShallow( Handle<ObjectTree> ) override;

public:
  LocalScheduler( Runtime& rt )
    : Scheduler( rt )
  {}

  virtual EvalTag schedule( Handle<Fix> top_level_job ) override;
};
