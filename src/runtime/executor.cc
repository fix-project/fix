#include <glog/logging.h>
#include <memory>
#include <string_view>
#include <vector>

#include "executor.hh"
#include "fixpointapi.hh"
#include "overload.hh"
#include "storage_exception.hh"

template<typename T>
using Result = Executor::Result<T>;

using namespace std;

Executor::Executor( size_t threads, weak_ptr<IRuntime> parent, optional<shared_ptr<Runner>> runner )
  : evaluator_( *this )
  , parent_( parent )
  , runner_( runner.has_value() ? runner.value()
                                : make_shared<WasmRunner>( parent.lock()->labeled( "compile-elf" ) ) )
{
  for ( size_t i = 0; i < threads; i++ ) {
    threads_.emplace_back( [&]() {
      fixpoint::storage = &storage_;
      run();
    } );
  }
}

Executor::~Executor()
{
  todo_.close();
  for ( size_t i = 0; i < threads_.size(); i++ ) {
    threads_[i].join();
  }
}

void Executor::run()
{
  try {
    Handle<Relation> next;
    while ( true ) {
      todo_ >> next;
      progress( next );
    }
  } catch ( ChannelClosed& ) {
    return;
  }
}

void Executor::progress( Handle<Relation> relation )
{
  {
    auto parent = parent_.lock();
    if ( parent && parent->contains( relation ) ) {
      return;
    }
  }

  auto result = evaluator_.relate( relation );
  if ( !result ) {
    todo_ << relation;
  }
}

std::optional<BlobData> Executor::get_or_delegate( Handle<Named> goal )
{
  if ( storage_.contains( goal ) ) {
    return storage_.get( goal );
  }

  auto parent = parent_.lock();
  if ( parent ) {
    return parent->get( goal );
  } else {
    // we have no way to get this
    throw HandleNotFound( goal );
  }
}

std::optional<TreeData> Executor::get_or_delegate( Handle<AnyTree> goal )
{
  if ( storage_.contains( goal ) ) {
    return storage_.get( goal );
  }

  auto parent = parent_.lock();
  if ( parent ) {
    return parent->get( goal );
  } else {
    // we have no way to get this
    throw HandleNotFound( handle::fix( goal ) );
  }
}

Result<Object> Executor::get_or_delegate( Handle<Relation> goal )
{
  if ( storage_.contains( goal ) ) {
    return storage_.get( goal );
  }
  auto parent = parent_.lock();
  if ( parent ) {
    return parent->get( goal );
  } else {
    todo_ << goal;
    return {};
  }
}

Result<Value> Executor::load( Handle<Value> value )
{
  return value.visit<Result<Value>>( overload {
    [&]( Handle<Blob> x ) -> Result<Blob> {
      if ( x.contains<Literal>() )
        return x;

      if ( storage_.contains( x.unwrap<Named>() ) )
        return x;

      return get_or_delegate( x.unwrap<Named>() ).transform( [&]( auto ) { return x; } );
    },
    [&]( Handle<ValueTree> x ) -> Result<Value> {
      if ( storage_.contains( x ) )
        return x;

      return get_or_delegate( x ).and_then( [&]( auto t ) -> Result<Value> {
        bool all_loaded = false;
        for ( const auto& handle : t->span() ) {
          all_loaded
            &= handle::extract<Value>( handle ).and_then( [&]( auto h ) { return load( h ); } ).has_value();
        }
        if ( all_loaded ) {
          return x;
        } else {
          return {};
        }
      } );
    },
    [&]( auto x ) { return x; },
  } );
}

Result<Object> Executor::apply( Handle<ObjectTree> combination )
{
  VLOG( 2 ) << "Apply " << combination;
  Handle<Apply> goal( combination );

  if ( storage_.contains( goal ) ) {
    return storage_.get( goal );
  } else {
    auto parent = parent_.lock();
    if ( parent && parent->contains( goal ) ) {
      VLOG( 2 ) << "Relation existed " << goal;
      auto res = parent->get( goal );

      handle::data( res.value() )
        .visit<void>( overload {
          [&]( Handle<Named> n ) { load( n ); },
          [&]( Handle<ValueTree> t ) { load( t ); },
          [&]( Handle<ObjectTree> ) { throw std::runtime_error( "unimplemented" ); },
          [&]( Handle<ExpressionTree> ) { throw std::runtime_error( "unimplemented" ); },
          [&]( Handle<Literal> ) {},
          [&]( Handle<Relation> ) {},
        } );

      return res;
    }
  }

  TreeData tree;
  {
    auto live = live_.write();
    if ( live->contains( goal ) ) {
      return {};
    }
    live->insert( goal );
  }
  tree = storage_.get( combination );

  auto result = runner_->apply( combination, tree );

  storage_.create( goal, result );

  {
    auto parent = parent_.lock();
    if ( parent )
      parent->put( goal, result );
  }

  auto live = live_.write();
  live->erase( goal );
  return result;
}

Result<Value> Executor::evalStrict( Handle<Object> expression )
{
  Handle<Eval> goal( expression );

  if ( storage_.contains( goal ) ) {
    return storage_.get( goal ).unwrap<Value>();
  } else {
    auto parent = parent_.lock();
    if ( parent && parent->contains( goal ) ) {
      VLOG( 2 ) << "Relation existed " << goal;
      auto res = parent->get( goal )->unwrap<Value>();
      res.visit<void>( overload {
        [&]( Handle<Blob> n ) { load( n ); },
        [&]( Handle<ValueTree> t ) { load( t ); },
        []( auto ) {},
      } );

      return res;
    }
  }

  auto result = evaluator_.evalStrict( expression );
  if ( !result )
    return {};

  storage_.create( goal, *result );
  auto parent = parent_.lock();
  if ( parent )
    parent->put( goal, *result );

  return result;
}

Result<Object> Executor::evalShallow( Handle<Object> expression )
{
  return evaluator_.evalShallow( expression );
}

Result<ValueTree> Executor::mapEval( Handle<ObjectTree> tree )
{
  {
    bool ready = true;
    auto data = storage_.get( tree );
    for ( const auto& x : data->span() ) {
      auto obj = x.unwrap<Expression>().unwrap<Object>();
      Handle<Eval> eval( obj );
      auto result = get_or_delegate( eval ); // this will start the relevant eval if it's not ready
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
      Handle<Eval> eval( obj );
      values[i] = get_or_delegate( eval ).value().unwrap<Value>();
    }

    if ( tree.is_tag() ) {
      return storage_.create( std::make_shared<OwnedTree>( std::move( values ) ) ).unwrap<ValueTree>().tag();
    } else {
      return storage_.create( std::make_shared<OwnedTree>( std::move( values ) ) ).unwrap<ValueTree>();
    }
  }
}

Result<ObjectTree> Executor::mapReduce( Handle<ExpressionTree> tree )
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

Result<ValueTree> Executor::mapLift( Handle<ValueTree> tree )
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

  auto objs = OwnedMutTree::allocate( data->size() );
  for ( size_t i = 0; i < data->size(); i++ ) {
    auto x = data->at( i );
    auto exp = x.unwrap<Expression>();
    objs[i] = evaluator_.reduce( exp ).value();
  }

  if ( tree.is_tag() ) {
    return storage_.create( std::make_shared<OwnedTree>( std::move( objs ) ) ).unwrap<ValueTree>().tag();
  } else {
    return storage_.create( std::make_shared<OwnedTree>( std::move( objs ) ) ).unwrap<ValueTree>();
  }
}

std::optional<BlobData> Executor::get( Handle<Named> name )
{
  if ( storage_.contains( name ) ) {
    return storage_.get( name );
  }
  throw HandleNotFound( name );
};

std::optional<TreeData> Executor::get( Handle<AnyTree> name )
{
  if ( storage_.contains( name ) ) {
    return storage_.get( name );
  }
  throw HandleNotFound( handle::fix( name ) );
};

std::optional<Handle<Object>> Executor::get( Handle<Relation> name )
{
  if ( storage_.contains( name ) ) {
    return storage_.get( name );
  }
  if ( threads_.size() == 0 ) {
    throw HandleNotFound( name );
  }
  todo_ << name;
  return {};
}

void Executor::put( Handle<Named> name, BlobData data )
{
  storage_.create( data, name );
}

void Executor::put( Handle<AnyTree> name, TreeData data )
{
  storage_.create( data, name );
}

void Executor::put( Handle<Relation> name, Handle<Object> data )
{
  storage_.create( name, data );
}

bool Executor::contains( Handle<Named> handle )
{
  return storage_.contains( handle );
}

bool Executor::contains( Handle<AnyTree> handle )
{
  return storage_.contains( handle );
}

bool Executor::contains( Handle<Relation> handle )
{
  return storage_.contains( handle );
}

Handle<Fix> Executor::labeled( const std::string_view label )
{
  return storage_.labeled( label );
};

bool Executor::contains( const std::string_view label )
{
  return storage_.contains( label );
}

template<FixType T>
requires std::convertible_to<Handle<T>, Handle<Object>>
void Executor::visit_minrepo( Handle<T> handle,
                              std::function<void( Handle<AnyDataType> )> visitor,
                              std::unordered_set<Handle<Object>> visited )
{
  if ( visited.contains( handle ) )
    return;
  if constexpr ( std::same_as<T, Literal> )
    return;

  if constexpr ( Handle<T>::is_fix_sum_type ) {
    if constexpr ( not( std::same_as<T, Thunk> or std::same_as<T, ValueTreeRef>
                        or std::same_as<T, ObjectTreeRef> ) )
      std::visit( [&]( const auto x ) { visit_minrepo( x, visitor, visited ); }, handle.get() );
  } else {
    if constexpr ( FixTreeType<T> ) {
      auto tree = get_or_delegate( handle );
      for ( const auto& element : tree.value()->span() ) {
        visit_minrepo( handle::extract<Object>( element ).value(), visitor, visited );
      }
    }
    VLOG( 2 ) << "visiting " << handle;
    visitor( handle );
    visited.insert( handle );
  }
}

void Executor::load_to_storage( Handle<AnyDataType> handle )
{
  handle::data( handle ).visit<void>( overload { []( Handle<Literal> ) { return; },
                                                 [&]( auto h ) {
                                                   if ( !contains( h ) )
                                                     put( h, get_or_delegate( h ).value() );
                                                 } } );
}

void Executor::load_minrepo( Handle<ObjectTree> combination )
{
  visit_minrepo( combination, [&]( Handle<AnyDataType> h ) { load_to_storage( h ); } );
}
