#include <stdio.h>

#include "test.hh"

using namespace std;

uint32_t fix_add( uint32_t a, uint32_t b, Handle add_elf )
{
  Handle add = thunk( tree( { blob( "unused" ), add_elf, a, b } ) );
  auto& rt = Runtime::get_instance();
  (void)a, (void)b;
  Handle result = rt.eval( add );
  uint32_t x = -1;
  memcpy( &x, result.literal_blob().data(), sizeof( uint32_t ) );
  return x;
}

void check_add( uint32_t a, uint32_t b, Handle add, string name )
{
  uint32_t sum_blob = fix_add( a, b, add );
  printf( "%s: %u + %u = %u\n", name.c_str(), a, b, sum_blob );
  if ( sum_blob != a + b ) {
    fprintf( stderr, "%s: got %u + %u = %u, expected %u.\n", name.c_str(), a, b, sum_blob, a + b );
    exit( 1 );
  }
}

void check_add( uint32_t a, uint32_t b )
{
  static Handle addblob = compile( file( "testing/wasm-examples/addblob.wasm" ) );
  static Handle add_simple = compile( file( "testing/wasm-examples/add-simple.wasm" ) );
  check_add( a, b, addblob, "addblob" );
  check_add( a, b, add_simple, "add-simple" );
}

int main( int, char** )
{
  auto& rt = Runtime::get_instance();
  rt.storage().deserialize();
  for ( size_t i = 0; i < 32; i++ ) {
    check_add( random(), random() );
  }
  check_add( 0, 0 );
  check_add( pow( 2, 32 ) - 5, pow( 2, 31 ) + 12 );
}
