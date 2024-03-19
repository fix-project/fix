#pragma once

#include "dependency_graph.hh"
#include "evaluator.hh"
#include "handle.hh"
#include "repository.hh"
#include "runner.hh"

inline thread_local std::vector<Handle<AnyDataType>> works_;
inline thread_local std::optional<Handle<Relation>> current_;

class Executor;
class Scheduler;
class OnePassScheduler;
class LocalFirstScheduler;

class Relater
  : public MultiWorkerRuntime
  , FixRuntime
{
  friend class Executor;
  friend class OnePassScheduler;
  friend class LocalFirstScheduler;

private:
  SharedMutex<DependencyGraph> graph_ {};
  FixEvaluator evaluator_;
  RuntimeStorage storage_ {};
  Repository repository_ {};
  std::shared_ptr<Scheduler> scheduler_ {};

  SharedMutex<std::vector<std::weak_ptr<IRuntime>>> remotes_ {};
  std::shared_ptr<IRuntime> local_ {};

  // Return result if completed, otherwise add a dependency from current_ to goal
  Result<Object> get_or_block( Handle<Relation> goal );
  // Return the list of doable works. After the function returns, graph_ is modified such that
  // finishing all the doable works would recursivly finish the top level relation.
  std::vector<Handle<AnyDataType>> relate( Handle<Relation> );

  template<FixType T>
  void get_from_repository( Handle<T> handle );

public:
  Relater( size_t threads = std::thread::hardware_concurrency(),
           std::optional<std::shared_ptr<Runner>> runner = {},
           std::optional<std::shared_ptr<Scheduler>> scheduler = {} );

  virtual void add_worker( std::shared_ptr<IRuntime> ) override;
  Handle<Value> execute( Handle<Relation> x );

  virtual Result<Fix> load( Handle<AnyDataType> value ) override;
  virtual Result<AnyTree> load( Handle<AnyTreeRef> value ) override;
  virtual Handle<AnyTreeRef> ref( Handle<AnyTree> tree ) override;
  virtual Result<Object> apply( Handle<ObjectTree> combination ) override;
  virtual Result<Value> evalStrict( Handle<Object> expression ) override;
  virtual Result<Object> evalShallow( Handle<Object> expression ) override;
  virtual Result<ValueTree> mapEval( Handle<ObjectTree> tree ) override;
  virtual Result<ObjectTree> mapReduce( Handle<ExpressionTree> tree ) override;
  virtual Result<ValueTree> mapLift( Handle<ValueTree> tree ) override;

  virtual std::optional<BlobData> get( Handle<Named> name ) override;
  virtual std::optional<TreeData> get( Handle<AnyTree> name ) override;
  virtual std::optional<Handle<Object>> get( Handle<Relation> name ) override;
  virtual void put( Handle<Named> name, BlobData data ) override;
  virtual void put( Handle<AnyTree> name, TreeData data ) override;
  virtual void put( Handle<Relation> name, Handle<Object> data ) override;
  virtual bool contains( Handle<Named> handle ) override;
  virtual bool contains( Handle<AnyTree> handle ) override;
  virtual bool contains( Handle<Relation> handle ) override;
  virtual std::optional<Handle<AnyTree>> contains( Handle<AnyTreeRef> handle ) override;
  virtual bool contains( const std::string_view label ) override;
  virtual Handle<Fix> labeled( const std::string_view label ) override;

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
      if constexpr ( std::same_as<T, Encode> ) {
        Handle<Thunk> thunk
          = handle.template visit<Handle<Thunk>>( []( auto s ) { return s.template unwrap<Thunk>(); } );
        thunk.visit<void>(
          overload { [&]( Handle<Application> a ) { visit( a.unwrap<ExpressionTree>(), visitor, visited ); },
                     []( auto ) {} } );
      }

      if constexpr ( not( std::same_as<T, Thunk> or std::same_as<T, Encode> ) )
        std::visit( [&]( const auto x ) { visit( x, visitor, visited ); }, handle.get() );

    } else if constexpr ( std::same_as<T, ValueTreeRef> or std::same_as<T, ObjectTreeRef> ) {
      return;
    } else {
      if constexpr ( FixTreeType<T> ) {
        // Having the handle means that the data presents in storage
        auto tree = get( handle );
        for ( const auto& element : tree.value()->span() ) {
          visit( element, visitor, visited );
        }
      }
      VLOG( 2 ) << "visiting " << handle;
      visitor( handle );
      visited.insert( handle );
    }
  }

  template<FixType T>
  void visit_full( Handle<T> handle,
                   std::function<void( Handle<AnyDataType> )> visitor,
                   std::unordered_set<Handle<Fix>> visited = {} )
  {
    if ( visited.contains( handle ) )
      return;
    if constexpr ( std::same_as<T, Literal> )
      return;

    if constexpr ( Handle<T>::is_fix_sum_type ) {
      if constexpr ( std::same_as<T, Relation> ) {
        auto target = get( handle );
        std::visit( [&]( const auto x ) { visit_full( x, visitor, visited ); }, target->get() );

        auto lhs = handle.template visit<Handle<Object>>(
          overload { []( Handle<Apply> h ) { return h.unwrap<ObjectTree>(); },
                     []( Handle<Eval> h ) { return h.unwrap<Object>(); } } );
        std::visit( [&]( const auto x ) { visit_full( x, visitor, visited ); }, lhs.get() );

        VLOG( 2 ) << "visiting " << handle;
        visitor( handle );
        visited.insert( handle );
      }

    } else if constexpr ( std::same_as<T, ValueTreeRef> or std::same_as<T, ObjectTreeRef> ) {
      return;
    } else {
      if constexpr ( FixTreeType<T> ) {
        auto tree = get( handle );
        for ( const auto& element : tree.value()->span() ) {
          visit_full( element, visitor, visited );
        }
      }
      VLOG( 2 ) << "visiting " << handle;
      visitor( handle );
      visited.insert( handle );
    }
  }

  Repository& get_repository() { return repository_; }
};
