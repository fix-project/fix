#include <stdio.h>

#include "../tester/tester-utils.cc"
#include "test.hh"

using namespace std;

void test( void )
{
  auto& rt = Runtime::get_instance();
  rt.storage().deserialize();
  Handle curry = thunk( tree( {
    blob( "unused" ),
    compile( file( "applications-prefix/src/applications-build/curry/curry.wasm" ) ),
    compile( file( "testing/wasm-examples/add-simple.wasm" ) ),
    Handle( 2 ),
  } ) );
  Handle curried = rt.eval( curry );

  Handle add_one = thunk( tree( {
    blob( "unused" ),
    curried,
    Handle( 1 ),
  } ) );
  Handle add_one_result = rt.eval( add_one );

  Handle add_two = thunk( tree( {
    blob( "unused" ),
    add_one_result,
    Handle( 3 ),
  } ) );
  Blob add_two_result = rt.storage().get_blob( rt.eval( add_two ) );

  uint32_t result;
  memcpy( &result, add_two_result.data(), sizeof( uint32_t ) );

  if ( result == 4 ) {
    exit( 0 );
  }
  printf( "Result did not match: %d != 4\n", result );
}
