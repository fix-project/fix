#include <stdio.h>

#include "test.hh"

using namespace std;

auto rt = ReadOnlyTester::init();

uint32_t fix_fib( uint32_t x )
{
  static auto addblob = compile( *rt, file( *rt, "testing/wasm-examples/addblob.wasm" ) );
  static auto fib = compile( *rt, file( *rt, "testing/wasm-examples/fib.wasm" ) );
  auto thunk = Handle<Application>(
    handle::upcast( tree( *rt, blob( *rt, "unused" ), fib, Handle<Literal>( x ), addblob ) ) );
  auto result = rt->executor().execute( Handle<Eval>( thunk ) );
  uint32_t y = -1;

  auto res = result.try_into<Blob>().and_then( []( auto h ) { return h.template try_into<Literal>(); } );
  if ( res ) {
    memcpy( &y, res->data(), sizeof( uint32_t ) );
  } else {
    throw runtime_error( "Invalid add result." );
  }
  return y;
}

uint32_t fib( uint32_t x )
{
  static unordered_map<uint32_t, uint32_t> memos {};
  if ( x < 2 ) {
    return 1;
  }
  if ( memos.contains( x ) ) {
    return memos.at( x );
  }
  uint32_t result = fib( x - 1 ) + fib( x - 2 );
  memos[x] = result;
  return result;
}

void check_fib( uint32_t x )
{
  uint32_t result = fix_fib( x );
  printf( "fib(%u) = %u\n", x, result );
  if ( result != fib( x ) ) {
    fprintf( stderr, "got fib(%u) = %u, expected %u.\n", x, result, fib( x ) );
    exit( 1 );
  }
}

void test( void )
{
  for ( ssize_t i = 15; i >= 0; i-- ) {
    check_fib( i );
  }
}
