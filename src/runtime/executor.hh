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
  : protected FixRuntime
  , public IRuntime
{
protected:
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
            std::optional<std::shared_ptr<Runner>> runner = {} );

  ~Executor();

  Handle<Value> execute( Handle<Relation> x )
  {
    if ( storage_.contains( x ) ) {
      return storage_.get( x ).unwrap<Value>();
    }
    todo_ << x;
    Handle<Object> current = storage_.wait( x );
    while ( not current.contains<Value>() ) {
      current = storage_.wait( Handle<Eval>( current ) );
    }
    return current.unwrap<Value>();
  }

  template<FixType T>
  void visit( Handle<T> root,
              std::function<void( Handle<Fix> )> visitor,
              std::unordered_set<Handle<Fix>> visited = {} );
  std::vector<Handle<Fix>> minrepo( Handle<Fix> handle );
  void load_to_storage( Handle<Fix> handle );
  void load_minrepo( Handle<ObjectTree> handle );
  std::pair<Runner::BlobMap, Runner::TreeMap> collect_minrepo( Handle<ObjectTree> handle );

  RuntimeStorage& storage() { return storage_; }

protected:
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
  virtual bool contains( Handle<Named> handle ) override;
  virtual bool contains( Handle<AnyTree> handle ) override;
  virtual bool contains( Handle<Relation> handle ) override;
  virtual Handle<Fix> labeled( const std::string_view label ) override;

  /* }@ */
};
