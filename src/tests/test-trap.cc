#include <stdio.h>

#include "test.hh"

using namespace std;

int main( int, char** )
{
  auto& rt = Runtime::get_instance();
  rt.storage().deserialize();
  rt.storage().get_tree( rt.eval( thunk( tree( {
    blob( "unused" ),
    compile( file( "testing/wasm-examples/trap.wasm" ) ),
  } ) ) ) );
}
