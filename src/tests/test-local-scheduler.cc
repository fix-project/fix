#include "bptree-helper.hh"
#include "handle.hh"
#include "relater.hh"
#include "scheduler.hh"
#include "storage_exception.hh"

#include <cstdio>
#include <memory>
#include <stdexcept>

using namespace std;

auto scheduler = make_shared<LocalScheduler>();
auto rt = make_shared<Relater>( 0, make_shared<PointerRunner>(), scheduler );
size_t fib_called = 0;
size_t bptree_get_called = 0;
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

Handle<Selection> selection( Handle<Object> arg, uint64_t i )
{
  OwnedMutTree tree = OwnedMutTree::allocate( 2 );
  tree[0] = arg;
  tree[1] = Handle<Literal>( (uint64_t)i );
  return rt->create( std::make_shared<OwnedTree>( std::move( tree ) ) )
    .visit<Handle<Selection>>( overload {
      []( Handle<ExpressionTree> ) -> Handle<Selection> { throw std::runtime_error( "Invalid select." ); },
      []( auto x ) { return Handle<Selection>( Handle<ObjectTree>( x ) ); },
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

Handle<Object> bptree_get( Handle<ObjectTree> combination )
{
  bptree_get_called++;

  auto data = rt->get( combination ).value();
  int key( data->at( 3 ).unwrap<Expression>().unwrap<Object>().unwrap<Value>().unwrap<Blob>().unwrap<Literal>() );
  LOG( INFO ) << "finding bptree key " << key;

  bool isleaf;
  vector<int> keys;
  data->at( 1 ).unwrap<Expression>().unwrap<Object>().unwrap<Value>().unwrap<Blob>().visit<void>(
    overload { [&]( Handle<Literal> l ) {
                isleaf = l.data()[0];
                auto ptr = reinterpret_cast<const int*>( l.data() + 1 );
                keys.assign( ptr, ptr + l.size() / sizeof( int ) );
              },
               [&]( Handle<Named> n ) {
                 auto d = rt->get( n ).value();
                 isleaf = d->span().data()[0];
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

void test_bptree_get( void )
{
  BPTree bptree( 4 );
  for ( size_t i = 0; i < 4; i++ ) {
    bptree.insert( i, to_string( i ) );
  }
  auto t = bptree::to_storage( rt->get_storage(), bptree );

  auto select1 = scheduler->schedule(
    Handle<Eval>( application( bptree_get, Handle<Strict>( selection( t, 0 ) ), t, 1_literal32 ) ) );
  auto select7 = scheduler->schedule(
    Handle<Eval>( application( bptree_get, Handle<Strict>( selection( t, 0 ) ), t, 7_literal32 ) ) );

  CHECK_EQ( select1.has_value(), true );
  CHECK_EQ( select1.value().unwrap<Value>(), Handle<Value>( Handle<Literal>( to_string( 1 ) ) ) );
  CHECK_EQ( select7.has_value(), true );
  CHECK_EQ( select7.value().unwrap<Value>(), Handle<Value>( Handle<Literal>( "Not found." ) ) );
}

void test( void )
{
  test_add();
  test_bptree_get();
  test_fib();
}
