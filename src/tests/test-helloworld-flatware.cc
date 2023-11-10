#include <stdio.h>

#include "test.hh"

using namespace std;

void test( void )
{
  auto& rt = Runtime::get_instance();
  rt.storage().deserialize();
  Tree result = rt.storage().get_tree( rt.eval( thunk( tree( {
    blob( "unused" ),
    compile(
      file( "applications-prefix/src/applications-build/flatware/examples/helloworld/helloworld-fixpoint.wasm" ) ),
  } ) ) ) );

  uint32_t x = -1;
  memcpy( &x, rt.storage().get_blob( result[0] ).data(), sizeof( uint32_t ) );
  if ( x != 0 ) {
    fprintf( stderr, "Return failed, returned %d != 0\n", x );
    exit( 1 );
  }
  auto out = rt.storage().get_blob( result[1] );
  if ( std::string_view( out.data(), out.size() ) != "Hello, World!" ) {
    fprintf( stderr, "Output did not match expected 'Hello, World!'\n" );
    exit( 1 );
  }
}
