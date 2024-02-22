#pragma once

#include <memory>
#include <thread>
#include <unordered_set>
#include <vector>

#include "channel.hh"
#include "dependency_graph.hh"
#include "evaluator.hh"
#include "handle.hh"
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
  SharedMutex<DependencyGraph> graph_ {};

  FixEvaluator evaluator_;
  std::optional<std::reference_wrapper<IRuntime>> parent_;
  std::shared_ptr<Runner> runner_;

public:
  Executor( size_t threads = std::thread::hardware_concurrency(),
            std::optional<std::reference_wrapper<IRuntime>> parent = {},
            std::optional<std::shared_ptr<Runner>> runner = {} );

  ~Executor();

  Handle<Value> execute( Handle<Relation> x )
  {
    {
      if ( parent_.has_value() && parent_->get().contains( x ) ) {
        VLOG( 2 ) << "Relation existed " << x;
        return parent_->get().get( x )->unwrap<Value>();
      }
    }

    VLOG( 1 ) << "Relation does not exit " << x.content;
    todo_ << x;
    Handle<Object> current = storage_.wait( x );
    while ( not current.contains<Value>() ) {
      current = storage_.wait( Handle<Eval>( current ) );
    }
    return current.unwrap<Value>();
  }

  template<FixType T>
  requires std::convertible_to<Handle<T>, Handle<Object>>
  void visit_minrepo( Handle<T> root,
                      std::function<void( Handle<AnyDataType> )> visitor,
                      std::unordered_set<Handle<Object>> visited = {} );

  template<FixType T>
  void visit( Handle<T> handle,
              std::function<void( Handle<AnyDataType> )> visitor,
              std::unordered_set<Handle<Fix>> visited = {} )
  {
    if ( visited.contains( handle ) )
      return;
    if constexpr ( std::same_as<T, Literal> )
      return;

    if constexpr ( Handle<T>::is_fix_sum_type ) {
      if ( not( std::same_as<T, ValueTreeRef> or std::same_as<T, ObjectTreeRef> ) ) {
        std::visit( [&]( const auto x ) { visit( x, visitor, visited ); }, handle.get() );
      }
      if constexpr ( std::same_as<T, Relation> ) {
        auto target = get_or_delegate( handle, handle );
        std::visit( [&]( const auto x ) { visit( x, visitor, visited ); }, target->get() );

        auto lhs = handle.template visit<Handle<Object>>(
          overload { []( Handle<Apply> h ) { return h.unwrap<ObjectTree>(); },
                     []( Handle<Eval> h ) { return h.unwrap<Object>(); } } );
        std::visit( [&]( const auto x ) { visit( x, visitor, visited ); }, lhs.get() );

        VLOG( 2 ) << "visiting " << handle;
        visitor( handle );
        visited.insert( handle );
      }

    } else {
      if constexpr ( FixTreeType<T> ) {
        auto tree = get_or_delegate( handle );
        for ( const auto& element : tree.value()->span() ) {
          visit( element, visitor, visited );
        }
      }
      VLOG( 2 ) << "visiting " << handle;
      visitor( handle );
      visited.insert( handle );
    }
  }

  void load_to_storage( Handle<AnyDataType> handle );
  void load_minrepo( Handle<ObjectTree> handle );

  RuntimeStorage& storage() { return storage_; }

private:
  void run();
  void progress( Handle<Relation> relation );

  std::optional<BlobData> get_or_delegate( Handle<Named> goal );
  std::optional<TreeData> get_or_delegate( Handle<AnyTree> goal );
  Result<Object> get_or_delegate( Handle<Relation> goal, Handle<Relation> blocked );

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
  virtual bool contains( const std::string_view label ) override;

  /* }@ */
};
