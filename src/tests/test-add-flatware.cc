#include <stdio.h>

#include "test.hh"

using namespace std;

uint32_t fix_add( char a, char b, Handle add_elf )
{
  Handle add = thunk(
    tree( { blob( "unused" ), add_elf, tree( { blob( "add" ), blob( { &a, 1 } ), blob( { &b, 1 } ) } ) } ) );
  auto& rt = Runtime::get_instance();
  (void)a, (void)b;
  Handle result = rt.eval( add );
  Tree tree = rt.storage().get_tree( result );
  Handle sum = tree.at( 0 );
  uint32_t x = -1;
  memcpy( &x, sum.literal_blob().data(), sizeof( uint32_t ) );
  return x;
}

void check_add( uint8_t a, uint8_t b, Handle add, string name )
{
  uint32_t sum_blob = fix_add( a, b, add );
  printf( "%s: %u + %u = %u\n", name.c_str(), (uint8_t)a, (uint8_t)b, sum_blob );
  if ( sum_blob != (uint32_t)a + (uint32_t)b ) {
    fprintf(
      stderr, "%s: got %u + %u = %u, expected %u.\n", name.c_str(), a, b, sum_blob, (uint32_t)a + (uint32_t)b );
    exit( 1 );
  }
}

void check_add( uint8_t a, uint8_t b )
{
  static Handle add_flatware
    = compile( file( "applications-prefix/src/applications-build/flatware/examples/add/add-fixpoint.wasm" ) );
  check_add( a, b, add_flatware, "add-fixpoint" );
}

void test( void )
{
  auto& rt = Runtime::get_instance();
  rt.storage().deserialize();
  for ( size_t i = 0; i < 32; i++ ) {
    check_add( random(), random() );
  }
  check_add( 0, 0 );
}
