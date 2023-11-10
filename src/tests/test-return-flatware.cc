#include <stdio.h>

#include "test.hh"

using namespace std;

void test( void )
{
  auto& rt = Runtime::get_instance();
  rt.storage().deserialize();
  Tree result = rt.storage().get_tree( rt.eval( thunk( tree( {
    blob( "unused" ),
    compile( file( "applications-prefix/src/applications-build/flatware/examples/return3/return3-fixpoint.wasm" ) ),
  } ) ) ) );

  auto blob = rt.storage().get_blob( result[0] );
  CHECK( blob.size() == 4 );
  uint32_t x = *( reinterpret_cast<const uint32_t*>( blob.data() ) );
  if ( x != 3 ) {
    fprintf( stderr, "Return failed, returned %d != 3\n", x );
    exit( 1 );
  }
}
