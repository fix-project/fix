#include <glog/logging.h>
#include <memory>
#include <vector>

#include "executor.hh"
#include "overload.hh"
#include "storage_exception.hh"

template<typename T>
using Result = Executor::Result<T>;

Executor::Executor( size_t threads, std::weak_ptr<IRuntime> parent, std::shared_ptr<Runner> runner )
  : evaluator_( *this )
  , parent_( parent )
  , runner_( runner )
{
  for ( size_t i = 0; i < threads; i++ ) {
    threads_.emplace_back( [&]() { run(); } );
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
      return get_or_delegate( x.unwrap<Named>() ).transform( [&]( auto ) { return x; } );
    },
    [&]( Handle<ValueTree> x ) { return get_or_delegate( x ).transform( [&]( auto ) { return x; } ); },
    [&]( auto x ) { return x; },
  } );
}

Result<Object> Executor::apply( Handle<ObjectTree> combination )
{
  Handle<Apply> goal( combination );
  TreeData tree;
  {
    if ( storage_.contains( goal ) ) {
      return storage_.get( goal );
    }
    tree = storage_.get( combination );
    // releasing the lock here should be safe, since the TreeData is immutable and its memory is managed
  }
  auto result = runner_->apply( combination, tree );
  storage_.create( goal, result );
  return result;
}

Result<Value> Executor::evalStrict( Handle<Object> expression )
{
  Handle<Eval> goal( expression );
  {
    if ( storage_.contains( goal ) ) {
      return storage_.get_relation( goal );
    }
  }
  auto result = evaluator_.evalStrict( expression );
  if ( !result )
    return {};
  storage_.create( goal, *result );
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
    {
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
  {
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
  {
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
