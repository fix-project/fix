#include <stdio.h>

#include "test.hh"

using namespace std;

void test( void )
{
  auto& rt = Runtime::get_instance();
  rt.storage().deserialize();
  Handle input = flatware_input( compile(
    file( "applications-prefix/src/applications-build/flatware/examples/helloworld/helloworld-fixpoint.wasm" ) ) );
  Handle result_handle = rt.eval( input );
  Tree result = rt.storage().get_tree( result_handle );

  uint32_t x = -1;
  memcpy( &x, rt.storage().get_blob( result[0] ).data(), sizeof( uint32_t ) );
  if ( x != 0 ) {
    fprintf( stderr, "Return failed, returned %d != 0\n", x );
    exit( 1 );
  }
  auto out = rt.storage().get_blob( result[2] );
  if ( std::string_view( out.data(), out.size() ) != "Hello, World!\n" ) {
    fprintf( stderr, "Output did not match expected 'Hello, World!'\n" );
    exit( 1 );
  }
}
