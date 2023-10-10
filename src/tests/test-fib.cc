#include <stdio.h>

#include "test.hh"

using namespace std;

uint32_t fix_fib( uint32_t x )
{
  static Handle addblob = compile( file( "testing/wasm-examples/addblob.wasm" ) );
  static Handle fib = compile( file( "testing/wasm-examples/fib.wasm" ) );
  Handle t = thunk( tree( { blob( "unused" ), fib, x, addblob } ) );
  auto& rt = Runtime::get_instance();
  Handle result = rt.eval( t );
  uint32_t y = -1;
  memcpy( &y, result.literal_blob().data(), sizeof( uint32_t ) );
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

int main( int, char** )
{
  auto& rt = Runtime::get_instance();
  rt.storage().deserialize();
  for ( ssize_t i = 15; i >= 0; i-- ) {
    check_fib( i );
  }
}
