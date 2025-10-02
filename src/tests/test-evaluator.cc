#include <iostream>

#include <glog/logging.h>
#include <stdexcept>

#include "bptree-helper.hh"
#include "evaluator.hh"
#include "overload.hh"
#include "runtimestorage.hh"

using namespace std;

RuntimeStorage storage;
size_t mapreduce_called = 0;

/**
 * A single-threaded Fix Runtime which executes in a stackful manner.  Its evaluation functions are guaranteed to
 * return values, not nullopt.
 */
class FakeRuntime : private FixRuntime
{
  FixEvaluator evaluator_;

public:
  FakeRuntime()
    : evaluator_( *this )
  {}

  template<typename T>
  using Result = FixRuntime::Result<T>;

public:
  Handle<Value> eval( Handle<Fix> target ) { return evaluator_.eval( target ).value(); }

private:
  virtual Result<Blob> load( Handle<Blob> value ) { return value; }
  virtual Result<AnyTree> load( Handle<AnyTree> value )
  {

    auto res = storage.get_handle( value );
    // std::cout << "FakeRuntime::load " << value << " " << res << std::endl;
    return res;
  }

  virtual Result<AnyTree> load( Handle<AnyTreeRef> value ) { return storage.contains( value ); }
  virtual Result<AnyTree> loadShallow( Handle<AnyTree> value ) { return load( value ); }

  virtual Handle<AnyTreeRef> ref( Handle<AnyTree> t )
  {
    return t.visit<Handle<AnyTreeRef>>( overload {
      []( Handle<ExpressionTree> ) -> Handle<AnyTreeRef> { throw runtime_error( "Not reffable" ); },
      []( Handle<ObjectTree> t ) { return t.into<ObjectTreeRef>( 0 ); },
      []( Handle<ValueTree> t ) { return t.into<ValueTreeRef>( 0 ); },
    } );
  }

  virtual Result<Object> select( Handle<ObjectTree> combination )
  {
    Handle<Relation> select = Handle<Think>( Handle<Thunk>( Handle<Selection>( combination ) ) );

    if ( storage.contains( select ) ) {
      return storage.get( select );
    }

    auto tree = storage.get( combination );

    auto h = tree->at( 0 ).unwrap<Expression>().unwrap<Object>();
    auto size = handle::size( h );
    auto i = size_t(
      tree->at( 1 ).unwrap<Expression>().unwrap<Object>().unwrap<Value>().unwrap<Blob>().unwrap<Literal>() );

    if ( i >= size ) {
      LOG( INFO ) << "Selecting " << i << " from" << h;
      throw std::runtime_error( "Select OOB" );
    }

    // h is one of BlobRef, ValueTreeRef or ObjectTreeRef
    auto result = h.visit<Result<Object>>( overload {
      [&]( Handle<ObjectTreeRef> x ) {
        return loadShallow( storage.contains( x ).value() )
          .transform( [&]( Handle<AnyTree> x ) { return storage.get( x ); } )
          .transform( [&]( TreeData d ) { return d->span()[i].unwrap<Expression>().unwrap<Object>(); } );
      },
      [&]( Handle<Value> v ) {
        return v.visit<Result<Object>>( overload {
          [&]( Handle<Literal> l ) { return Handle<Literal>( l.view().substr( i, 1 ) ); },
          [&]( Handle<BlobRef> x ) {
            return load( x.unwrap<Blob>() )
              .transform( [&]( Handle<Blob> x ) { return storage.get( x.unwrap<Named>() ); } )
              .transform( [&]( BlobData b ) { return Handle<Literal>( b->span()[i] ); } );
          },
          [&]( Handle<ValueTreeRef> x ) {
            return loadShallow( storage.contains( x ).value() )
              .transform( [&]( Handle<AnyTree> x ) { return storage.get( x ); } )
              .transform( [&]( TreeData d ) { return d->span()[i].unwrap<Expression>().unwrap<Object>(); } );
          },
          []( auto ) -> Result<Object> { throw std::runtime_error( "Invalid select" ); },
        } );
      },
      []( auto ) -> Result<Object> { throw std::runtime_error( "Invalid select" ); },
    } );

    return result;
    return {};
  }

  virtual Result<Object> apply( Handle<ObjectTree> combination )
  {
    Handle<Think> apply
      = Handle<Think>( Handle<Thunk>( Handle<Application>( Handle<ExpressionTree>( combination ) ) ) );

    if ( storage.contains( apply ) ) {
      return storage.get_relation( apply );
    }

    auto span = storage.get( combination );
    Handle<Value> function = span->at( 0 )
                               .try_into<Expression>()
                               .and_then( []( const auto x ) { return x.template try_into<Object>(); } )
                               .and_then( []( const auto x ) { return x.template try_into<Value>(); } )
                               .value();
    Handle<Literal> literal
      = function.try_into<Blob>().and_then( []( const Handle<Blob> x ) { return x.try_into<Literal>(); } ).value();
    auto f = ( Handle<Object>( * )( Handle<Object> ) )(uint64_t)literal;
    auto result = f( combination );
    storage.create( result, apply );
    return result;
  };

  virtual Result<Value> evalStrict( Handle<Object> obj )
  {
    Handle<Eval> eval( obj );
    if ( storage.contains( eval ) ) {
      return storage.get_relation( eval );
    }
    auto result = evaluator_.evalStrict( obj ).value();
    storage.create( result, eval );
    return result;
  }

  virtual Result<Object> force( Handle<Thunk> thunk ) { return evaluator_.force( thunk ); };
  virtual Result<ValueTree> mapEval( Handle<ObjectTree> tree )
  {
    auto objs = storage.get( tree );
    auto vals = OwnedMutTree::allocate( objs->size() );
    for ( size_t i = 0; i < objs->size(); i++ ) {
      vals[i] = evalStrict( objs->at( i ).unwrap<Expression>().unwrap<Object>() ).value();
    }
    return storage.create( std::make_shared<OwnedTree>( std::move( vals ) ) ).unwrap<ValueTree>();
  };

  virtual Result<ObjectTree> mapReduce( Handle<ExpressionTree> tree )
  {
    mapreduce_called++;

    auto exprs = storage.get( tree );
    auto objs = OwnedMutTree::allocate( exprs->size() );
    for ( size_t i = 0; i < exprs->size(); i++ ) {
      objs[i] = evaluator_.reduce( exprs->at( i ).unwrap<Expression>() ).value();
    }
    return storage.create_tree<ObjectTree>( std::make_shared<OwnedTree>( std::move( objs ) ) );
  }

  virtual Result<ValueTree> mapLift( Handle<ValueTree> tree )
  {
    auto vals = storage.get( tree );
    auto new_vals = OwnedMutTree::allocate( vals->size() );
    for ( size_t i = 0; i < vals->size(); i++ ) {
      new_vals[i] = evaluator_.lift( vals->at( i ).unwrap<Expression>().unwrap<Object>().unwrap<Value>() ).value();
    }
    return storage.create( std::make_shared<OwnedTree>( std::move( new_vals ) ) ).unwrap<ValueTree>();
  }

  virtual Result<ObjectTree> mapEvalShallow( Handle<ObjectTree> tree )
  {
    auto objs = storage.get( tree );
    auto vals = OwnedMutTree::allocate( objs->size() );
    for ( size_t i = 0; i < objs->size(); i++ ) {
      vals[i] = evaluator_.evalShallow( objs->at( i ).unwrap<Expression>().unwrap<Object>() ).value();
    }
    return storage.create( std::make_shared<OwnedTree>( std::move( vals ) ) )
      .visit<Handle<ObjectTree>>( overload {
        []( Handle<ValueTree> v ) { return Handle<ObjectTree>( v ); },
        []( Handle<ObjectTree> o ) { return o; },
        []( Handle<ExpressionTree> ) -> Handle<ObjectTree> { throw runtime_error( "Unreachable" ); },
      } );
  }
};

template<FixHandle... Args>
Handle<Application> application( Handle<Object> ( *f )( Handle<Object> ), Args... args )
{
  return storage.construct_tree<ExpressionTree>( Handle<Literal>( (uint64_t)f ), args... );
}

template<FixHandle Arg>
Handle<Selection> selection( Arg arg, uint64_t i )
{
  return storage.construct_tree<ObjectTree>( arg, Handle<Literal>( (uint64_t)i ) );
}

Handle<Object> add( Handle<Object> combination )
{
  auto data = storage.get( combination.unwrap<ObjectTree>() );
  uint64_t x(
    data->at( 1 ).unwrap<Expression>().unwrap<Object>().unwrap<Value>().unwrap<Blob>().unwrap<Literal>() );
  uint64_t y(
    data->at( 2 ).unwrap<Expression>().unwrap<Object>().unwrap<Value>().unwrap<Blob>().unwrap<Literal>() );
  LOG( INFO ) << "adding " << x << " and " << y;
  return Handle<Literal>( x + y );
}

Handle<Object> fib( Handle<Object> combination )
{
  auto data = storage.get( combination.unwrap<ObjectTree>() );
  uint64_t x(
    data->at( 1 ).unwrap<Expression>().unwrap<Object>().unwrap<Value>().unwrap<Blob>().unwrap<Literal>() );
  LOG( INFO ) << "calculating fib of " << x;
  if ( x < 2 ) {
    return Handle<Literal>( x );
  } else {
    auto a = Handle<Strict>( application( fib, Handle<Literal>( x - 1 ) ) );
    auto b = Handle<Strict>( application( fib, Handle<Literal>( x - 2 ) ) );
    return application( add, a, b );
  }
}

Handle<Object> bptree_get( Handle<Object> combination )
{
  auto data = storage.get( combination.unwrap<ObjectTree>() );
  int key( data->at( 3 ).unwrap<Expression>().unwrap<Object>().unwrap<Value>().unwrap<Blob>().unwrap<Literal>() );
  LOG( INFO ) << "finding bptree key " << key;

  bool isleaf;
  vector<int> keys;
  data->at( 1 ).unwrap<Expression>().unwrap<Object>().unwrap<Value>().unwrap<Blob>().visit<void>(
    overload { [&]( Handle<Literal> l ) {
                isleaf = static_cast<bool>( l.data()[0] );
                auto ptr = reinterpret_cast<const int*>( l.data() + 1 );
                keys.assign( ptr, ptr + l.size() / sizeof( int ) );
              },
               [&]( Handle<Named> n ) {
                 auto d = storage.get( n );
                 isleaf = static_cast<bool>( d->span().data()[0] );
                 auto ptr = reinterpret_cast<const int*>( d->span().data() + 1 );
                 keys.assign( ptr, ptr + d->span().size() / sizeof( int ) );
               } } );

  auto childrenordata = data->at( 2 ).unwrap<Expression>().unwrap<Object>().unwrap<Value>().unwrap<ValueTreeRef>();

  if ( isleaf ) {
    LOG( INFO ) << "is leaf ";
    auto pos = upper_bound( keys.begin(), keys.end(), key );
    auto idx = pos - keys.begin() - 1;
    if ( pos != keys.begin() && *( pos - 1 ) == key ) {
      return selection( childrenordata, idx + 1 );
    } else {
      return Handle<Literal>( "Not found." );
    }
  } else {
    LOG( INFO ) << "not leaf ";
    size_t idx = upper_bound( keys.begin(), keys.end(), key ) - keys.begin();
    auto nextlevel = selection( childrenordata, idx + 1 );
    auto keysentry = selection( nextlevel, 0 );
    return application(
      bptree_get, Handle<Strict>( keysentry ), Handle<Shallow>( nextlevel ), Handle<Literal>( key ) );
  }
}

void test( void )
{
  FakeRuntime rt;
  auto sum = rt.eval( application( add, 1_literal64, 2_literal64 ) );
  CHECK_EQ( sum, Handle<Value>( 3_literal64 ) );
  CHECK_EQ( mapreduce_called, 0 );
  auto fib10 = rt.eval( application( fib, 10_literal64 ) );
  CHECK_EQ( fib10, Handle<Value>( 55_literal64 ) );
  CHECK_EQ( mapreduce_called, 9 );

  BPTree<int, string> bptree( 4 );
  for ( int i = 0; i < 10; i++ ) {
    bptree.insert( i, to_string( i ) );
  }

  auto t = bptree::to_storage( storage, bptree );
  auto select1 = rt.eval( application( bptree_get, Handle<Strict>( selection( t, 0 ) ), t, 1_literal32 ) );
  CHECK_EQ( select1, Handle<Value>( Handle<Literal>( to_string( 1 ) ) ) );
}
