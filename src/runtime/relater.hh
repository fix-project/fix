#pragma once

#include "dependency_graph.hh"
#include "handle.hh"
#include "repository.hh"
#include "runner.hh"
#include "runtimestorage.hh"
#include <unordered_set>

class Executor;
class Scheduler;
class LocalScheduler;
class SketchGraphScheduler;
class BasePass;
class RelaterTest;

class Relater : public MultiWorkerRuntime
{
  friend class Executor;
  friend class LocalScheduler;
  friend class SketchGraphScheduler;
  friend class BasePass;
  friend class RelaterTest;

private:
  std::atomic<bool> top_level_done_ { true };
  Handle<Relation> top_level {};
  Handle<Value> result {};
  bool finish_top_level( Handle<Relation>, Handle<Object> );

  SharedMutex<DependencyGraph> graph_ {};
  RuntimeStorage storage_ {};
  Repository repository_ {};
  std::shared_ptr<Scheduler> scheduler_ {};

  SharedMutex<std::vector<std::weak_ptr<IRuntime>>> remotes_ {};
  std::shared_ptr<IRuntime> local_ {};

  // Set of Think( Apply ) that are waiting on I/Os
  SharedMutex<std::unordered_set<Handle<Relation>>> occupying_resource_ {};
  SharedMutex<size_t> available_memory_ {};
  bool pre_occupy_ {};

  template<FixType T>
  void get_from_repository( Handle<T> handle );

public:
  Relater( size_t threads = std::thread::hardware_concurrency(),
           std::optional<std::shared_ptr<Runner>> runner = {},
           std::optional<std::shared_ptr<Scheduler>> scheduler = {},
           bool pre_occupy_ = false );

  virtual void add_worker( std::shared_ptr<IRuntime> ) override;
  Handle<Value> execute( Handle<Relation> x );

  virtual std::optional<BlobData> get( Handle<Named> name ) override;
  virtual std::optional<TreeData> get( Handle<AnyTree> name ) override;
  virtual std::optional<Handle<Object>> get( Handle<Relation> name ) override;
  virtual std::optional<TreeData> get_shallow( Handle<AnyTree> name ) override;
  virtual std::optional<Handle<AnyTree>> get_handle( Handle<AnyTree> name ) override;
  virtual void put( Handle<Named> name, BlobData data ) override;
  virtual void put( Handle<AnyTree> name, TreeData data ) override;
  virtual void put_shallow( Handle<AnyTree> name, TreeData data ) override;
  virtual void put( Handle<Relation> name, Handle<Object> data ) override;
  virtual bool contains( Handle<Named> handle ) override;
  virtual bool contains( Handle<AnyTree> handle ) override;
  virtual bool contains_shallow( Handle<AnyTree> handle ) override;
  virtual bool contains( Handle<Relation> handle ) override;
  virtual std::optional<Handle<AnyTree>> contains( Handle<AnyTreeRef> handle ) override;
  virtual bool contains( const std::string_view label ) override;
  virtual Handle<Fix> labeled( const std::string_view label ) override;

  Handle<AnyTreeRef> ref( Handle<AnyTree> );
  Handle<AnyTree> unref( Handle<AnyTreeRef> );

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
        if ( contains( handle ) ) {
          auto target = get( handle ).value();
          std::visit( [&]( const auto x ) { visit_full( x, visitor, visited ); }, target.get() );
        }

        auto lhs = handle.template visit<Handle<Object>>(
          overload { []( Handle<Think> s ) { return s.unwrap<Thunk>(); },
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
        if ( contains( handle ) ) {
          auto tree = get( handle ).value();
          for ( const auto& element : tree->span() ) {
            visit_full( element, visitor, visited );
          }
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

  RuntimeStorage& get_storage() { return storage_; }
  Repository& get_repository() { return repository_; }
  virtual std::unordered_set<Handle<AnyDataType>> data() const override { return repository_.data(); }
  virtual absl::flat_hash_set<Handle<Dependee>> get_forward_dependencies( Handle<Relation> blocked ) override
  {
    return graph_.read()->get_forward_dependencies( blocked );
  }
  std::shared_ptr<IRuntime> get_local() { return local_; }

  std::optional<Handle<Object>> run( Handle<Relation> );

  bool occupy_resource( Handle<Think> relation );
  void unoccupy_resource( Handle<Relation> relation );
};
