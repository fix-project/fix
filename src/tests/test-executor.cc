#include "relater.hh"
#include <glog/logging.h>

using namespace std;

Relater rt( std::thread::hardware_concurrency(), std::make_shared<PointerRunner>() );

template<FixHandle... Args>
Handle<Application> application( Handle<Object> ( *f )( Handle<ObjectTree> ), Args... args )
{
  OwnedMutTree tree = OwnedMutTree::allocate( sizeof...( args ) + 1 );
  tree[0] = Handle<Literal>( (uint64_t)f );
  size_t i = 1;
  (
    [&] {
      tree[i] = args;
      i++;
    }(),
    ... );
  return rt.create( std::make_shared<OwnedTree>( std::move( tree ) ) ).visit<Handle<Application>>( []( auto x ) {
    return Handle<Application>( Handle<ExpressionTree>( x ) );
  } );
}

Handle<Object> add( Handle<ObjectTree> combination )
{
  auto data = rt.get( combination ).value();
  uint64_t x(
    data->at( 1 ).unwrap<Expression>().unwrap<Object>().unwrap<Value>().unwrap<Blob>().unwrap<Literal>() );
  uint64_t y(
    data->at( 2 ).unwrap<Expression>().unwrap<Object>().unwrap<Value>().unwrap<Blob>().unwrap<Literal>() );
  /* LOG( INFO ) << x << " + " << y << " = " << x + y; */
  return Handle<Literal>( x + y );
}

Handle<Object> fib( Handle<ObjectTree> combination )
{
  auto data = rt.get( combination ).value();
  uint64_t x(
    data->at( 1 ).unwrap<Expression>().unwrap<Object>().unwrap<Value>().unwrap<Blob>().unwrap<Literal>() );
  /* LOG( INFO ) << "fib(" << x << ")"; */
  if ( x < 2 ) {
    return Handle<Literal>( x );
  } else {
    auto a = Handle<Strict>( application( fib, Handle<Literal>( x - 1 ) ) );
    auto b = Handle<Strict>( application( fib, Handle<Literal>( x - 2 ) ) );
    return application( add, a, b );
  }
}

Handle<Object> manyfib( Handle<ObjectTree> combination )
{
  auto data = rt.get( combination );
  uint64_t x(
    data.value()->at( 1 ).unwrap<Expression>().unwrap<Object>().unwrap<Value>().unwrap<Blob>().unwrap<Literal>() );
  auto tree = OwnedMutTree::allocate( x );
  for ( uint64_t i = 0; i < tree.size(); i++ ) {
    tree[i] = application( fib, Handle<Literal>( i ) );
  }
  return handle::tree_unwrap<ObjectTree>( rt.create( std::make_shared<OwnedTree>( std::move( tree ) ) ) );
}

void test( void )
{
  if ( getenv( "FIB" ) ) {
    auto fibs_h = rt.execute( Handle<Eval>( application( manyfib, 94_literal64 ) ) ).unwrap<ValueTree>();
    auto fibs = rt.get( fibs_h );
    uint64_t a = 0;
    uint64_t b = 1;
    for ( size_t i = 0; i < fibs.value()->size(); i++ ) {
      CHECK_EQ( fibs.value()->at( i ), Handle<Fix>( Handle<Literal>( a ) ) );
      b = a + b;
      a = b - a;
    }
  } else {
    for ( size_t i = 0; i < 10000; i++ ) {
      uint64_t a = rand();
      uint64_t b = rand();
      auto sum = rt.execute( Handle<Eval>( application( add, Handle<Literal>( a ), Handle<Literal>( b ) ) ) );

      CHECK_EQ( sum, Handle<Value>( Handle<Literal>( a + b ) ) );
    }
  }
}
