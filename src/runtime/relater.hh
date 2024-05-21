#pragma once

#include "dependency_graph.hh"
#include "evaluator.hh"
#include "handle.hh"
#include "repository.hh"
#include "runner.hh"

inline thread_local std::vector<Handle<AnyDataType>> works_;
inline thread_local Handle<Relation> current_;
inline thread_local DependencyGraph sketch_graph_;
inline thread_local absl::flat_hash_set<Handle<Relation>> must_be_local_;
inline thread_local bool replaced_;

class Executor;
class Scheduler;
class LocalFirstScheduler;
class HintScheduler;
class BasePass;
class RelaterTest;

class Relater
  : public MultiWorkerRuntime
  , FixRuntime
{
  friend class Executor;
  friend class LocalFirstScheduler;
  friend class HintScheduler;
  friend class BasePass;
  friend class RelaterTest;

private:
  SharedMutex<DependencyGraph> graph_ {};
  FixEvaluator evaluator_;
  RuntimeStorage storage_ {};
  Repository repository_ {};
  std::shared_ptr<Scheduler> scheduler_ {};

  SharedMutex<std::vector<std::weak_ptr<IRuntime>>> remotes_ {};
  std::shared_ptr<IRuntime> local_ {};

  // Return the list of doable works. After the function returns, graph_ is modified such that
  // finishing all the doable works would recursivly finish the top level relation.
  void relate( Handle<Relation> );

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

  virtual std::optional<Info> get_info() override
  {
    // Info to be exposed to other nodes
    auto info = local_->get_info();
    info->link_speed = 7.5;
    return info;
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
      if constexpr ( std::same_as<T, BlobRef> ) {
        return;
      }

      if constexpr ( std::same_as<T, Relation> ) {
        auto target = get( handle );
        std::visit( [&]( const auto x ) { visit_full( x, visitor, visited ); }, target->get() );

        auto lhs = handle.template visit<Handle<Object>>(
          overload { []( Handle<Apply> h ) { return h.unwrap<ObjectTree>(); },
                     []( Handle<Eval> h ) { return h.unwrap<Object>(); } } );
        std::visit( [&]( const auto x ) { visit_full( x, visitor, visited ); }, lhs.get() );

        VLOG( 3 ) << "visiting " << handle;
        visitor( handle );
        visited.insert( handle );
      } else {
        std::visit( [&]( const auto x ) { visit_full( x, visitor, visited ); }, handle.get() );
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
      VLOG( 3 ) << "visiting " << handle;
      visitor( handle );
      visited.insert( handle );
    }
  }

  // Stop visiting recursively if visitor( Tree ) returns true
  template<FixType T>
  void early_stop_visit_minrepo( Handle<T> handle,
                                 std::function<bool( Handle<AnyDataType> )> visitor,
                                 std::unordered_set<Handle<Fix>> visited = {} )
  {
    if ( visited.contains( handle ) )
      return;
    if constexpr ( std::same_as<T, Literal> )
      return;

    if constexpr ( Handle<T>::is_fix_sum_type ) {
      if constexpr ( not( std::same_as<T, Thunk> or std::same_as<T, Encode> or std::same_as<T, BlobRef> ) )
        std::visit( [&]( const auto x ) { early_stop_visit_minrepo( x, visitor, visited ); }, handle.get() );

    } else if constexpr ( std::same_as<T, ValueTreeRef> or std::same_as<T, ObjectTreeRef> ) {
      return;
    } else {
      VLOG( 3 ) << "visiting " << handle;
      auto res = visitor( handle );
      visited.insert( handle );

      if ( res )
        return;

      if constexpr ( FixTreeType<T> ) {
        if ( storage_.contains( handle ) ) {
          auto tree = get( handle );
          for ( const auto& element : tree.value()->span() ) {
            early_stop_visit_minrepo( element, visitor, visited );
          }
        }
      }
    }
  }

  Repository& get_repository() { return repository_; }
  virtual std::unordered_set<Handle<AnyDataType>> data() const override { return repository_.data(); }
  virtual absl::flat_hash_set<Handle<AnyDataType>> get_forward_dependencies( Handle<Relation> blocked ) override
  {
    return graph_.read()->get_forward_dependencies( blocked );
  }

  void merge_sketch_graph( Handle<Relation> r, absl::flat_hash_set<Handle<Relation>>& unblocked );

  std::shared_ptr<IRuntime> get_local() { return local_; }
};
