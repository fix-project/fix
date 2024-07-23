#include <iostream>

#include <glog/logging.h>

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
  virtual Handle<AnyTreeRef> ref( Handle<AnyTree> t )
  {
    return t.visit<Handle<AnyTreeRef>>( overload {
      []( Handle<ExpressionTree> ) -> Handle<AnyTreeRef> { throw runtime_error( "Not reffable" ); },
      []( Handle<ObjectTree> t ) { return t.into<ObjectTreeRef>( 0 ); },
      []( Handle<ValueTree> t ) { return t.into<ValueTreeRef>( 0 ); },
    } );
  }

  virtual Result<Object> apply( Handle<ObjectTree> combination )
  {
    Handle<Apply> apply( combination );
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
  virtual Result<Object> evalShallow( Handle<Object> obj ) { return evaluator_.evalShallow( obj ); };
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
};

template<FixHandle... Args>
Handle<Application> application( Handle<Object> ( *f )( Handle<Object> ), Args... args )
{
  return storage.construct_tree<ExpressionTree>( Handle<Literal>( (uint64_t)f ), args... );
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

void test( void )
{
  FakeRuntime rt;
  auto sum = rt.eval( application( add, 1_literal64, 2_literal64 ) );
  CHECK_EQ( sum, Handle<Value>( 3_literal64 ) );
  CHECK_EQ( mapreduce_called, 0 );
  auto fib10 = rt.eval( application( fib, 10_literal64 ) );
  CHECK_EQ( fib10, Handle<Value>( 55_literal64 ) );
  CHECK_EQ( mapreduce_called, 9 );
}
