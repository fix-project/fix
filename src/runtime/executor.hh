#pragma once
#include <memory>
#include <thread>
#include <unordered_set>
#include <vector>

#include "channel.hh"
#include "evaluator.hh"
#include "interface.hh"
#include "mutex.hh"
#include "runner.hh"
#include "runtimestorage.hh"

class Executor
  : private FixRuntime
  , public IRuntime
{
  std::vector<std::thread> threads_ {};
  Channel<Handle<Relation>> todo_ {};
  RuntimeStorage storage_ {};
  SharedMutex<std::unordered_set<Handle<Relation>>> live_ {};

  FixEvaluator evaluator_;
  std::weak_ptr<IRuntime> parent_ {};
  std::shared_ptr<Runner> runner_;

public:
  Executor( size_t threads = std::thread::hardware_concurrency(),
            std::weak_ptr<IRuntime> parent = {},
            std::shared_ptr<Runner> runner = std::make_shared<WasmRunner>() );

  ~Executor();

  Handle<Value> execute( Handle<Relation> x )
  {
    if ( storage_.contains( x ) ) {
      return storage_.get( x ).unwrap<Value>();
    }
    todo_ << x;
    storage_.wait( [&] { return storage_.contains( x ); } );
    return storage_.get( x ).unwrap<Value>();
  }

private:
  void run();
  void progress( Handle<Relation> relation );

  std::optional<BlobData> get_or_delegate( Handle<Named> goal );
  std::optional<TreeData> get_or_delegate( Handle<AnyTree> goal );
  Result<Object> get_or_delegate( Handle<Relation> goal );

  /** @defgroup Implementation of FixRuntime
   * @{
   */

  virtual Result<Value> load( Handle<Value> value ) override;
  virtual Result<Object> apply( Handle<ObjectTree> combination ) override;
  virtual Result<Value> evalStrict( Handle<Object> expression ) override;
  virtual Result<Object> evalShallow( Handle<Object> expression ) override;
  virtual Result<ValueTree> mapEval( Handle<ObjectTree> tree ) override;
  virtual Result<ObjectTree> mapReduce( Handle<ExpressionTree> tree ) override;
  virtual Result<ValueTree> mapLift( Handle<ValueTree> tree ) override;

  /* }@ */

public:
  /** @defgroup Implementation of IRuntime
   * @{
   */

  virtual std::optional<BlobData> get( Handle<Named> name ) override;
  virtual std::optional<TreeData> get( Handle<AnyTree> name ) override;
  virtual std::optional<Handle<Object>> get( Handle<Relation> name ) override;
  virtual void put( Handle<Named> name, BlobData data ) override;
  virtual void put( Handle<AnyTree> name, TreeData data ) override;
  virtual void put( Handle<Relation> name, Handle<Object> data ) override;
  virtual bool contains( Handle<Fix> handle ) override;

  /* }@ */
};
