#include "handle.hh"
#include "relater.hh"
#include "scheduler.hh"
#include "storage_exception.hh"

#include <cstdio>
#include <memory>

using namespace std;

auto scheduler = make_shared<LocalScheduler>();
auto rt = make_shared<Relater>( 0, make_shared<PointerRunner>(), scheduler );
size_t fib_called = 0;
optional<Handle<Fix>> exception_caught {};

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
  return rt->create( std::make_shared<OwnedTree>( std::move( tree ) ) ).visit<Handle<Application>>( []( auto x ) {
    return Handle<Application>( Handle<ExpressionTree>( x ) );
  } );
}

Handle<Object> add( Handle<ObjectTree> combination )
{
  auto data = rt->get( combination ).value();
  uint64_t x(
    data->at( 1 ).unwrap<Expression>().unwrap<Object>().unwrap<Value>().unwrap<Blob>().unwrap<Literal>() );
  uint64_t y(
    data->at( 2 ).unwrap<Expression>().unwrap<Object>().unwrap<Value>().unwrap<Blob>().unwrap<Literal>() );
  /* LOG( INFO ) << x << " + " << y << " = " << x + y; */
  return Handle<Literal>( x + y );
}

Handle<Object> fib( Handle<ObjectTree> combination )
{
  fib_called++;

  auto data = rt->get( combination ).value();
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

void test_add( void )
{
  uint64_t a = rand();
  uint64_t b = rand();

  auto sum = Handle<Eval>( application( add, Handle<Literal>( a ), Handle<Literal>( b ) ) );
  auto res = scheduler->schedule( sum );

  CHECK_EQ( res.has_value(), true );
  CHECK_EQ( res.value().unwrap<Value>(), Handle<Value>( Handle<Literal>( a + b ) ) );
}

void test_fib( void )
{
  auto fib3 = Handle<Eval>( application( fib, Handle<Literal>( (uint64_t)3 ) ) );

  try {
    scheduler->schedule( fib3 );
  } catch ( StorageException& e ) {
    string handle = string( &e.what()[22], 64 );
    exception_caught = Handle<Fix>::forge( base16::decode( handle ) );
  }

  CHECK_EQ( fib_called, 1 );
  CHECK_EQ( exception_caught.has_value(), true );

  Handle<Fix> fib2 = Handle<Step>( Handle<Thunk>( application( fib, Handle<Literal>( (uint64_t)2 ) ) ) );
  CHECK_EQ( exception_caught.value(), fib2 );
}

void test( void )
{
  test_add();
  test_fib();
}
