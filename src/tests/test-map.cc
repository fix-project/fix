#include <stdio.h>

#include "test.hh"

using namespace std;

void test( void )
{
  auto& rt = Runtime::get_instance();
  rt.storage().deserialize();
  Handle t = thunk( tree( {
    blob( "unused" ),
    compile( file( "applications-prefix/src/applications-build/map/add_2_map.wasm" ) ),
    Handle( 0 ),
  } ) );
  Tree result = rt.storage().get_tree( rt.eval( t ) );
  uint32_t results[3];
  for ( size_t i = 0; i < 3; i++ ) {
    memcpy( &results[i], result[i].literal_blob().data(), sizeof( uint32_t ) );
  }
  if ( results[0] == 6 and results[1] == 10 and results[2] == 3 ) {
    exit( 0 );
  }
  printf( "Results did not match: {%d, %d, %d} != {6, 10, 3}\n", results[0], results[1], results[2] );
}
