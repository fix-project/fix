#include "relater.hh"
#include "executor.hh"
#include "handle.hh"
#include "handle_post.hh"
#include "scheduler.hh"
#include "storage_exception.hh"
#include "types.hh"
#include <memory>
#include <stdexcept>

using namespace std;

template<FixType T>
void Relater::get_from_repository( Handle<T> handle )
{
  if constexpr ( std::same_as<T, Literal> )
    return;

  if constexpr ( Handle<T>::is_fix_sum_type ) {
    if constexpr ( std::same_as<T, Relation> ) {
      auto rhs = repository_.get( handle ).value();
      get_from_repository( rhs );
      put( handle, rhs );
      return;
    }

    if constexpr ( not( std::same_as<T, Thunk> or std::same_as<T, Encode> or std::same_as<T, BlobRef> ) )
      std::visit( [&]( const auto x ) { get_from_repository( x ); }, handle.get() );

  } else if constexpr ( std::same_as<T, ValueTreeRef> or std::same_as<T, ObjectTreeRef> ) {
    return;
  } else {
    if constexpr ( FixTreeType<T> ) {
      // Having the handle means that the data presents in storage
      if ( storage_.contains( handle ) ) {
        return;
      }

      auto tree = repository_.get( handle );
      for ( const auto& element : tree.value()->span() ) {
        get_from_repository( element );
      }
      put( repository_.get_handle( handle ).value(), tree.value() );
    } else {
      auto named = handle::extract<Named>( handle ).value();
      if ( storage_.contains( named ) ) {
        return;
      }

      put( named, repository_.get( named ).value() );
    }
  }
}

Relater::Relater( size_t threads,
                  optional<shared_ptr<Runner>> runner,
                  optional<shared_ptr<Scheduler>> scheduler,
                  bool pre_occupy,
                  optional<size_t> repository_fix_table_size )
  : repository_( repository_fix_table_size.has_value() ? repository_fix_table_size.value() : 1000000 )
  , scheduler_( scheduler.has_value() ? move( scheduler.value() ) : make_shared<HintScheduler>() )
  , pre_occupy_( pre_occupy )
{
  available_memory_.write().get() = sysconf( _SC_PHYS_PAGES ) * sysconf( _SC_PAGE_SIZE );
  scheduler_->set_relater( *this );
  local_ = make_shared<Executor>( *this, threads, runner, pre_occupy );
}

void Relater::add_worker( shared_ptr<IRuntime> rmt )
{
  remotes_.write()->push_back( rmt );
}

Handle<Value> Relater::execute( Handle<Relation> r )
{
  if ( contains( r ) ) {
    return get( r ).value().unwrap<Value>();
  }

  top_level = r;
  bool expected = true;
  if ( top_level_done_.compare_exchange_strong( expected, false ) ) {
    if ( local_->get_info()->parallelism == 0 ) {
      remotes_.read()->front().lock()->get( r );
      // scheduler_->schedule( r );
    } else {
      local_->get( r );
    }

    top_level_done_.wait( false, std::memory_order_acquire );
    return result;
  } else {
    throw std::runtime_error( "Unexpected top level value." );
  }
}

Handle<Value> Relater::direct_execute( Handle<Relation> r )
{
  if ( contains( r ) ) {
    return get( r ).value().unwrap<Value>();
  }

  top_level = r;
  bool expected = true;
  if ( top_level_done_.compare_exchange_strong( expected, false ) ) {
    if ( local_->get_info()->parallelism == 0 ) {
      remotes_.read()->front().lock()->get( r );
    } else {
      scheduler_->schedule( r );
    }

    top_level_done_.wait( false, std::memory_order_acquire );
    return result;
  } else {
    throw std::runtime_error( "Unexpected top level value." );
  }
}

bool Relater::finish_top_level( Handle<Relation> name, Handle<Object> value )
{
  if ( !top_level_done_.load( std::memory_order_acquire ) and name == top_level ) {
    result = value.unwrap<Value>();
    top_level_done_.store( true, std::memory_order_release );
    top_level_done_.notify_all();

    return true;
  }

  return false;
}

optional<BlobData> Relater::get( Handle<Named> name )
{
  if ( storage_.contains( name ) ) {
    return storage_.get( name );
  } else if ( repository_.contains( name ) ) {
    get_from_repository( name );
    return storage_.get( name );
  }

  throw HandleNotFound( name );
}

optional<TreeData> Relater::get( Handle<AnyTree> name )
{
  if ( storage_.contains( name ) ) {
    return storage_.get( name );
  } else if ( repository_.contains( name ) ) {
    get_from_repository( name );
    return storage_.get( name );
  }

  throw HandleNotFound( handle::fix( name ) );
}

optional<TreeData> Relater::get_shallow( Handle<AnyTree> name )
{
  if ( storage_.contains_shallow( name ) ) {
    return storage_.get_shallow( name );
  } else if ( repository_.contains_shallow( name ) ) {
    auto tree = repository_.get_shallow( name ).value();
    storage_.create_tree_shallow( tree, name );
    return tree;
  }

  throw HandleNotFound( handle::fix( name ) );
}

optional<Handle<Object>> Relater::get( Handle<Relation> name )
{
  if ( storage_.contains( name ) ) {
    return storage_.get( name );
  } else if ( repository_.contains( name ) ) {
    get_from_repository( name );
    return storage_.get( name );
  }

  local_->get( name );
  return {};
}

optional<Handle<AnyTree>> Relater::get_handle( Handle<AnyTree> name )
{
  if ( storage_.contains( name ) || storage_.contains_shallow( name ) ) {
    return storage_.get_handle( name );
  } else if ( repository_.contains( name ) ) {
    return repository_.get_handle( name );
  }

  for ( auto& remote : remotes_.read().get() ) {
    auto locked_remote = remote.lock();
    if ( auto h = locked_remote->get_handle( name ); h.has_value() ) {
      return h;
    }
  }

  return {};
}

void Relater::put( Handle<Named> name, BlobData data )
{
  if ( !storage_.contains( name ) ) {
    storage_.create( data, name );
    absl::flat_hash_set<Handle<Relation>> unblocked;
    {
      auto graph = graph_.write();
      graph->finish( name, unblocked );
    }
    for ( auto x : unblocked ) {
      local_->get( x );
    }
  }
}

void Relater::put( Handle<AnyTree> name, TreeData data )
{
  if ( !storage_.contains( name ) ) {
    storage_.create( data, name );
    absl::flat_hash_set<Handle<Relation>> unblocked;
    {
      auto graph = graph_.write();
      name.visit<void>( [&]( auto h ) { graph->finish( h, unblocked ); } );
    }
    for ( auto x : unblocked ) {
      local_->get( x );
    }
  }
}

void Relater::put_shallow( Handle<AnyTree> name, TreeData data )
{
  if ( !storage_.contains_shallow( name ) ) {
    storage_.create_tree_shallow( data, name );
    absl::flat_hash_set<Handle<Relation>> unblocked;
    {
      auto graph = graph_.write();
      name.visit<void>( overload {
        [&]( Handle<ValueTree> t ) { graph->finish( Handle<ValueTreeRef>( t, data->size() ), unblocked ); },
        [&]( Handle<ObjectTree> t ) { graph->finish( Handle<ObjectTreeRef>( t, data->size() ), unblocked ); },
        []( Handle<ExpressionTree> ) { throw runtime_error( "Unreachable" ); },
      } );
    }
    for ( auto x : unblocked ) {
      local_->get( x );
    }
  }
}

void Relater::put( Handle<Relation> name, Handle<Object> data )
{
  if ( !storage_.contains( name ) ) {
    storage_.create( data, name );
    unoccupy_resource( name );

    if ( finish_top_level( name, data ) ) {
      return;
    }

    absl::flat_hash_set<Handle<Relation>> unblocked;
    {
      auto graph = graph_.write();
      graph->finish( name, unblocked );
    }
    for ( auto x : unblocked ) {
      local_->get( x );
    }
    for ( auto& remote : remotes_.read().get() ) {
      auto locked = remote.lock();
      if ( locked ) {
        locked->put( name, data );
      }
    }
  }
}

bool Relater::contains( Handle<Named> handle )
{
  return storage_.contains( handle ) || repository_.contains( handle );
}

bool Relater::contains( Handle<AnyTree> handle )
{
  return storage_.contains( handle ) || repository_.contains( handle );
}

bool Relater::contains_shallow( Handle<AnyTree> handle )
{
  return storage_.contains_shallow( handle ) || repository_.contains_shallow( handle );
}

bool Relater::contains( Handle<Relation> handle )
{
  return storage_.contains( handle ) || repository_.contains( handle );
}

std::optional<Handle<AnyTree>> Relater::contains( Handle<AnyTreeRef> handle )
{
  return storage_.contains( handle ).or_else( [&]() { return repository_.contains( handle ); } );
}

bool Relater::contains( const std::string_view label )
{
  return storage_.contains( label ) || repository_.contains( label );
}

Handle<Fix> Relater::labeled( const std::string_view label )
{
  if ( repository_.contains( label ) ) {
    return repository_.labeled( label );
  }
  return storage_.labeled( label );
}

Handle<AnyTreeRef> Relater::ref( Handle<AnyTree> tree )
{
  if ( !storage_.contains( tree ) ) {
    throw HandleNotFound( handle::fix( tree ) );
  }
  return storage_.ref( tree );
}

Handle<AnyTree> Relater::unref( Handle<AnyTreeRef> tree )
{
  auto h = contains( tree ).or_else( [&]() -> optional<Handle<AnyTree>> {
    for ( auto& remote : remotes_.read().get() ) {
      auto locked_remote = remote.lock();
      if ( auto h = locked_remote->contains( tree ); h.has_value() ) {
        return h;
      }
    }
    return {};
  } );

  if ( !h.has_value() ) {
    throw HandleNotFound( handle::fix( tree ) );
  }

  return h.value();
}

std::optional<Handle<Object>> Relater::run( Handle<Relation> r )
{
  return scheduler_->schedule( r );
}

bool Relater::occupy_resource( Handle<Think> relation )
{
  if ( pre_occupy_ ) {
    auto w = occupying_resource_.write();
    if ( w.get().size() >= local_->get_info()->parallelism ) {
      return false;
    }

    bool insert = w->insert( relation ).second;

    if ( insert ) {
      TreeData tree = storage_.get( relation.unwrap<Thunk>().unwrap<Application>().unwrap<ExpressionTree>() );
      auto rlimits = tree->at( 0 );

      auto limits = rlimits.unwrap<Expression>()
                      .unwrap<Object>()
                      .try_into<Value>()
                      .and_then( [&]( auto x ) { return x.template try_into<ValueTree>(); } )
                      .transform( [&]( auto x ) { return fixpoint::storage->get( x ); } );

      auto byte_requested
        = limits
            .and_then( [&]( auto x ) {
              return handle::extract<Literal>(
                x->at( 0 ).template unwrap<Expression>().template unwrap<Object>().template unwrap<Value>() );
            } )
            .transform( [&]( auto x ) { return uint64_t( x ); } )
            .value_or( 0 );

      {
        auto am = available_memory_.write();
        if ( am.get() < byte_requested ) {
          w.get().erase( relation );
          return false;
        }

        am.get() -= byte_requested;
      }
    }
  }
  return true;
}

void Relater::unoccupy_resource( Handle<Relation> relation )
{
  VLOG( 1 ) << "Unoccupying " << relation;
  if ( pre_occupy_ ) {
    if ( occupying_resource_.write()->erase( relation ) > 0 ) {
      auto unblock = no_resource_.pop();
      if ( unblock.has_value() ) {
        local_->get( unblock.value() );
      }
    }
  }
}
