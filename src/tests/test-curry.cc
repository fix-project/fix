#include <stdio.h>

#include "test.hh"

using namespace std;

Handle curry( Runtime& rt, Handle program, Handle num_args )
{
  return rt.eval( thunk( tree( {
    blob( "unused" ),
    compile( file( "applications-prefix/src/applications-build/curry/curry.wasm" ) ),
    program,
    num_args,
  } ) ) );
}

Handle apply_args( Runtime& rt, Handle curried, std::initializer_list<Handle> elements )
{
  for ( auto& e : elements ) {
    curried = rt.eval( thunk( tree( {
      blob( "unused" ),
      curried,
      e,
    } ) ) );
  }
  return curried;
}

void test_add_simple( Runtime& rt )
{
  Handle curried = curry( rt, compile( file( "testing/wasm-examples/add-simple.wasm" ) ), Handle( 2 ) );

  Handle curried_result = apply_args( rt, curried, { Handle( 1 ), Handle( 3 ) } );

  uint32_t blob_result = -1;
  memcpy( &blob_result, rt.storage().get_blob( curried_result ).data(), sizeof( uint32_t ) );

  if ( blob_result == 4 ) {
    exit( 0 );
  }
  printf( "test_add_simple: Result did not match: %d != 4\n", blob_result );
}

void test_add_as_encode( Runtime& rt )
{
  Handle curried = curry( rt,
                          tree( {
                            blob( "unused" ),
                            compile( file( "testing/wasm-examples/add-simple.wasm" ) ),
                          } ),
                          Handle( 2 ) );

  Handle curried_result = apply_args( rt, curried, { Handle( 1 ), Handle( 3 ) } );

  uint32_t blob_result = -1;
  memcpy( &blob_result, rt.storage().get_blob( curried_result ).data(), sizeof( uint32_t ) );

  if ( blob_result == 4 ) {
    exit( 0 );
  }
  printf( "test_add_as_encode: Result did not match: %d != 4\n", blob_result );
}

void test_curry_self( Runtime& rt )
{
  Handle curried
    = curry( rt, compile( file( "applications-prefix/src/applications-build/curry/curry.wasm" ) ), Handle( 2 ) );

  Handle curried_curry_result
    = apply_args( rt, curried, { compile( file( "testing/wasm-examples/add-simple.wasm" ) ), Handle( 2 ) } );

  Handle curried_add_result = apply_args( rt, curried_curry_result, { Handle( 1 ), Handle( 3 ) } );
  uint32_t result = -1;
  memcpy( &result, rt.storage().get_blob( curried_add_result ).data(), sizeof( uint32_t ) );

  if ( result == 4 ) {
    exit( 0 );
  }
  printf( "test_curry_self: Result did not match: %d != 4\n", result );
}

void test( void )
{
  auto& rt = Runtime::get_instance();
  rt.storage().deserialize();

  test_add_simple( rt );
  test_add_as_encode( rt );
  test_curry_self( rt );
}
