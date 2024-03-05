#include "relater.hh"
#include "evaluator.hh"
#include "executor.hh"
#include "handle.hh"
#include "handle_post.hh"
#include "scheduler.hh"
#include "storage_exception.hh"
#include "types.hh"
#include <memory>

using namespace std;

template<FixType T>
void get_from_repository( Handle<T> handle, Repository& rp, RuntimeStorage& rs )
{
  if constexpr ( std::same_as<T, Literal> )
    return;

  if constexpr ( Handle<T>::is_fix_sum_type ) {
    if constexpr ( std::same_as<T, Encode> ) {
      Handle<Thunk> thunk
        = handle.template visit<Handle<Thunk>>( []( auto s ) { return s.template unwrap<Thunk>(); } );
      thunk.visit<void>(
        overload { [&]( Handle<Application> a ) { get_from_repository( a.unwrap<ExpressionTree>(), rp, rs ); },
                   []( auto ) {} } );
    }

    if constexpr ( std::same_as<T, Relation> ) {
      auto rhs = rp.get( handle ).value();
      get_from_repository( rhs, rp, rs );
      rs.create( rp.get( handle ).value(), handle );
      return;
    }

    if constexpr ( not( std::same_as<T, Thunk> or std::same_as<T, Encode> or std::same_as<T, ValueTreeRef>
                        or std::same_as<T, ObjectTreeRef> ) )
      std::visit( [&]( const auto x ) { get_from_repository( x, rp, rs ); }, handle.get() );

  } else {
    if constexpr ( FixTreeType<T> ) {
      // Having the handle means that the data presents in storage
      if ( rs.contains( handle ) ) {
        return;
      }

      auto tree = rp.get( handle );
      for ( const auto& element : tree.value()->span() ) {
        get_from_repository( element, rp, rs );
      }
      rs.create( tree.value(), handle );
    } else {
      auto named = handle::extract<Named>( handle ).value();
      if ( rs.contains( named ) ) {
        return;
      }

      rs.create( rp.get( named ).value(), named );
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

Relater::Result<Value> Relater::load( Handle<Value> value )
{
  auto eval = Handle<Relation>( Handle<Eval>( Handle<Identification>( value ) ) );
  if ( eval != current_ ) {
    graph_.write()->add_dependency( current_.value(), eval );
  }
  works_.push_back( Handle<Eval>( Handle<Identification>( value ) ) );
  return {};
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

  if ( apply != current_ ) {
    graph_.write()->add_dependency( current_.value(), apply );
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

  auto result = evaluator_.evalStrict( expression );
  if ( result.has_value() ) {
    this->put( goal, result.value() );
  }
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
    auto result = get_or_block( Handle<Eval>( obj ) );
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
    values[i] = evaluator_.evalStrict( obj ).value();
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

vector<Handle<Relation>> Relater::relate( Handle<Relation> goal )
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
    get_from_repository( name, repository_, storage_ );
    return storage_.get( name );
  } else if ( remotes_.read()->size() > 0 ) {
    auto remote = remotes_.read()->front().lock();
    if ( remote ) {
      return remote->get( name );
    }
  }

  throw HandleNotFound( name );
}

optional<TreeData> Relater::get( Handle<AnyTree> name )
{
  if ( storage_.contains( name ) ) {
    return storage_.get( name );
  } else if ( repository_.contains( name ) ) {
    get_from_repository( name, repository_, storage_ );
    return storage_.get( name );
  } else if ( remotes_.read()->size() > 0 ) {
    auto remote = remotes_.read()->front().lock();
    if ( remote ) {
      return remote->get( name );
    }
  }

  throw HandleNotFound( handle::fix( name ) );
}

optional<Handle<Object>> Relater::get( Handle<Relation> name )
{
  if ( storage_.contains( name ) ) {
    return storage_.get( name );
  } else if ( repository_.contains( name ) ) {
    get_from_repository( name, repository_, storage_ );
    return storage_.get( name );
  }

  auto works = relate( name );
  if ( !works.empty() ) {
    scheduler_->schedule( works, name );
    return {};
  } else {
    return storage_.get( name );
  }
}

void Relater::put( Handle<Named> name, BlobData data )
{
  if ( !storage_.contains( name ) )
    storage_.create( data, name );
}
void Relater::put( Handle<AnyTree> name, TreeData data )
{
  if ( !storage_.contains( name ) )
    storage_.create( data, name );
}
void Relater::put( Handle<Relation> name, Handle<Object> data )
{
  VLOG( 1 ) << "Putting to relater name " << name;
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
      VLOG( 1 ) << "Putting to relater name to remote " << name;
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
