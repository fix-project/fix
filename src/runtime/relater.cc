#include "relater.hh"
#include "evaluator.hh"
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

    if constexpr ( not( std::same_as<T, Thunk> or std::same_as<T, Encode> or std::same_as<T, ValueTreeRef>
                        or std::same_as<T, ObjectTreeRef> or std::same_as<T, BlobRef> ) )
      std::visit( [&]( const auto x ) { get_from_repository( x ); }, handle.get() );

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
      put( handle, tree.value() );
    } else {
      auto named = handle::extract<Named>( handle ).value();
      if ( storage_.contains( named ) ) {
        return;
      }

      put( named, repository_.get( named ).value() );
    }
  }
}

Relater::Relater( size_t threads, optional<shared_ptr<Runner>> runner, optional<shared_ptr<Scheduler>> scheduler )
  : evaluator_( *this )
  , scheduler_( scheduler.has_value() ? move( scheduler.value() ) : make_shared<LocalFirstScheduler>() )
{
  scheduler_->set_relater( *this );
  local_ = make_shared<Executor>( *this, threads, runner );
}

Relater::Result<Fix> Relater::load( Handle<AnyDataType> handle )
{
  auto graph = graph_.write();

  auto contained = handle.visit<bool>(
    overload { [&]( Handle<Literal> ) { return true; }, [&]( auto h ) { return storage_.contains( h ); } } );

  if ( contained ) {
    return handle::fix( handle );
  } else {
    handle.visit<void>( overload {
      []( Handle<Literal> ) { throw std::runtime_error( "Unreachable" ); },
      [&]( Handle<AnyTree> t ) {
        t.visit<void>( [&]( auto h ) { graph->add_dependency( current_.value(), h ); } );
      },
      [&]( Handle<Named> n ) { graph->add_dependency( current_.value(), n ); },
      [&]( Handle<Relation> r ) { graph->add_dependency( current_.value(), r ); },
    } );
    works_.push_back( handle );
    return {};
  }
}

Relater::Result<AnyTree> Relater::load( Handle<AnyTreeRef> handle )
{
  auto h = contains( handle ).or_else( [&]() -> optional<Handle<AnyTree>> {
    for ( auto& remote : remotes_.read().get() ) {
      auto locked_remote = remote.lock();
      if ( auto h = locked_remote->contains( handle ); h.has_value() ) {
        return h;
      }
    }
    return {};
  } );

  if ( !h.has_value() ) {
    throw HandleNotFound( handle::fix( handle ) );
  }

  if ( storage_.contains( h.value() ) ) {
    return h.value();
  } else {
    auto d = h.value().visit<Handle<AnyDataType>>( []( auto h ) { return h; } );
    graph_.write()->add_dependency( current_.value(), d );
    works_.push_back( d );
    return {};
  }
}

Handle<AnyTreeRef> Relater::ref( Handle<AnyTree> tree )
{
  if ( !storage_.contains( tree ) ) {
    throw HandleNotFound( handle::fix( tree ) );
  }
  return storage_.ref( tree );
}

Relater::Result<Object> Relater::apply( Handle<ObjectTree> combination )
{
  auto apply = Handle<Relation>( Handle<Apply>( combination ) );

  if ( storage_.contains( apply ) ) {
    return storage_.get( apply );
  } else if ( contains( apply ) ) {
    // Relation exists in repository
    VLOG( 2 ) << "Relation existed " << apply;
    return get( apply );
  }

  {
    auto graph = graph_.write();
    if ( apply != current_ and !storage_.contains( apply ) ) {
      graph->add_dependency( current_.value(), apply );
    } else if ( storage_.contains( apply ) ) {
      return storage_.get( apply );
    }
  }
  works_.push_back( Handle<Apply>( combination ) );
  return {};
}

Relater::Result<Object> Relater::get_or_block( Handle<Relation> goal )
{
  auto prev_current = current_;

  current_ = goal;
  auto res = evaluator_.relate( goal );
  if ( !res.has_value() and prev_current.has_value() ) {
    graph_.write()->add_dependency( prev_current.value(), goal );
  }

  current_ = prev_current;
  return res;
}

Relater::Result<Value> Relater::evalStrict( Handle<Object> expression )
{
  Handle<Eval> goal( expression );

  if ( storage_.contains( goal ) ) {
    return storage_.get( goal ).unwrap<Value>();
  } else if ( contains( goal ) ) {
    VLOG( 2 ) << "Relation existed " << goal;
    return get( goal )->unwrap<Value>();
  }

  auto prev_current = current_;
  current_ = goal;

  auto result = evaluator_.evalStrict( expression );
  if ( result.has_value() ) {
    this->put( goal, result.value() );
  } else {
    auto graph = graph_.write();
    if ( prev_current.has_value() and prev_current.value() != Handle<Relation>( goal )
         and !storage_.contains( goal ) ) {
      graph->add_dependency( prev_current.value(), goal );
    } else if ( storage_.contains( goal ) ) {
      return storage_.get( goal ).unwrap<Value>();
    }
  }

  current_ = prev_current;
  return result;
}

Relater::Result<Object> Relater::evalShallow( Handle<Object> expression )
{
  return evaluator_.evalShallow( expression );
}

Relater::Result<ValueTree> Relater::mapEval( Handle<ObjectTree> tree )
{
  bool ready = true;
  auto data = storage_.get( tree );
  for ( const auto& x : data->span() ) {
    auto obj = x.unwrap<Expression>().unwrap<Object>();
    auto result = evalStrict( obj );
    if ( not result ) {
      ready = false;
    }
  }
  if ( not ready ) {
    return {};
  }

  auto values = OwnedMutTree::allocate( data->size() );
  for ( size_t i = 0; i < data->size(); i++ ) {
    auto x = data->at( i );
    auto obj = x.unwrap<Expression>().unwrap<Object>();
    values[i] = evalStrict( obj ).value();
  }

  if ( tree.is_tag() ) {
    return storage_.create( std::make_shared<OwnedTree>( std::move( values ) ) ).unwrap<ValueTree>().tag();
  } else {
    return storage_.create( std::make_shared<OwnedTree>( std::move( values ) ) ).unwrap<ValueTree>();
  }
}

Relater::Result<ObjectTree> Relater::mapReduce( Handle<ExpressionTree> tree )
{
  bool ready = true;
  TreeData data = storage_.get( tree );
  for ( const auto& x : data->span() ) {
    auto exp = x.unwrap<Expression>();
    auto result = evaluator_.reduce( exp );
    if ( not result ) {
      ready = false;
    }
  }
  if ( not ready ) {
    return {};
  }

  auto objs = OwnedMutTree::allocate( data->size() );
  for ( size_t i = 0; i < data->size(); i++ ) {
    auto exp = data->at( i ).unwrap<Expression>();
    objs[i] = evaluator_.reduce( exp ).value();
  }

  if ( tree.is_tag() ) {
    return handle::tree_unwrap<ObjectTree>( storage_.create( std::make_shared<OwnedTree>( std::move( objs ) ) ) )
      .tag();
  } else {
    return handle::tree_unwrap<ObjectTree>( storage_.create( std::make_shared<OwnedTree>( std::move( objs ) ) ) );
  }
}

Relater::Result<ValueTree> Relater::mapLift( Handle<ValueTree> tree )
{
  bool ready = true;
  auto data = storage_.get( tree );
  for ( const auto& x : data->span() ) {
    auto val = x.unwrap<Expression>().unwrap<Object>().unwrap<Value>();
    auto result = evaluator_.lift( val );
    if ( not result ) {
      ready = false;
    }
  }
  if ( not ready ) {
    return {};
  }

  auto vals = OwnedMutTree::allocate( data->size() );
  for ( size_t i = 0; i < data->size(); i++ ) {
    auto x = data->at( i );
    auto exp = x.unwrap<Expression>();
    vals[i] = evaluator_.reduce( exp ).value();
  }

  if ( tree.is_tag() ) {
    return storage_.create( std::make_shared<OwnedTree>( std::move( vals ) ) ).unwrap<ValueTree>().tag();
  } else {
    return storage_.create( std::make_shared<OwnedTree>( std::move( vals ) ) ).unwrap<ValueTree>();
  }
}

vector<Handle<AnyDataType>> Relater::relate( Handle<Relation> goal )
{
  works_ = {};
  get_or_block( goal );
  return works_;
}

void Relater::add_worker( shared_ptr<IRuntime> rmt )
{
  remotes_.write()->push_back( rmt );
}

Handle<Value> Relater::execute( Handle<Relation> r )
{
  get( r );
  Handle<Object> current = storage_.wait( r );
  while ( not current.contains<Value>() ) {
    current = storage_.wait( Handle<Eval>( current ) );
  }
  return current.unwrap<Value>();
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

optional<Handle<Object>> Relater::get( Handle<Relation> name )
{
  if ( storage_.contains( name ) ) {
    return storage_.get( name );
  } else if ( repository_.contains( name ) ) {
    get_from_repository( name );
    return storage_.get( name );
  }

  if ( local_->get_info()->parallelism == 0 ) {
    auto locked_remotes = remotes_.read();
    if ( locked_remotes->size() > 0 ) {
      for ( const auto& remote : locked_remotes.get() ) {
        auto locked_remote = remote.lock();
        if ( locked_remote ) {
          locked_remote->get( name );
        }
      }
    }
    return {};
  } else {
    auto works = relate( name );
    if ( !works.empty() ) {
      scheduler_->schedule( works, name );
      return {};
    } else {
      return storage_.get( name );
    }
  }
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
void Relater::put( Handle<Relation> name, Handle<Object> data )
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
