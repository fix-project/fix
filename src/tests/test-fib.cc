#include <memory>
#include <stdio.h>

#include "relater.hh"
#include "test.hh"

using namespace std;

uint32_t fix_fib( shared_ptr<Relater> rt, uint32_t x )
{
  static auto addblob = compile( *rt, file( *rt, "testing/wasm-examples/addblob.wasm" ) );
  static auto fib = compile( *rt, file( *rt, "testing/wasm-examples/fib.wasm" ) );
  auto thunk = Handle<Application>(
    handle::upcast( tree( *rt, limits( *rt, 1024 * 1024, 1024, 1 ), fib, Handle<Literal>( x ), addblob ) ) );
  auto result = rt->execute( Handle<Eval>( thunk ) );
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

void check_fib( shared_ptr<Relater> rt, uint32_t x )
{
  uint32_t result = fix_fib( rt, x );
  printf( "fib(%u) = %u\n", x, result );
  if ( result != fib( x ) ) {
    fprintf( stderr, "got fib(%u) = %u, expected %u.\n", x, result, fib( x ) );
    exit( 1 );
  }
}

void test( shared_ptr<Relater> rt )
{
  for ( ssize_t i = 15; i >= 0; i-- ) {
    check_fib( rt, i );
  }
}
