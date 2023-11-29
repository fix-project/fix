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

Handle apply_args( Runtime& rt, Handle curried, const initializer_list<Handle>& elements )
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

optional<uint32_t> unwrap_tag( Runtime& rt, const Handle& tag )
{
  if ( tag.get_content_type() != ContentType::Tag ) {
    cout << "unwrap_tag: Expected tag, got " << tag.get_content_type() << endl;
    return {};
  }
  auto& tag_state = rt.storage().get_tree( tag )[2];
  if ( not rt.storage().compare_handles( tag_state, Handle( true ) ) ) {
    cout << "unwrap_tag: Tag was not successful" << endl;
    return {};
  }

  auto& tag_contents = rt.storage().get_tree( tag )[0];
  uint32_t blob_result = -1;
  memcpy( &blob_result, rt.storage().get_blob( tag_contents ).data(), sizeof( uint32_t ) );
  return blob_result;
}

void test_add_simple( Runtime& rt )
{
  Handle curried = curry( rt, compile( file( "testing/wasm-examples/add-simple.wasm" ) ), Handle( 2 ) );

  Handle curried_result = apply_args( rt, curried, { Handle( 1 ), Handle( 3 ) } );

  auto optional_result = unwrap_tag( rt, curried_result );
  if ( not optional_result ) {
    printf( "test_add_simple: Result was not successful\n" );
  } else if ( optional_result.value() != 4 ) {
    printf( "test_add_simple: Result did not match: %d != 4\n", optional_result.value() );
  }
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

  auto optional_result = unwrap_tag( rt, curried_result );
  if ( not optional_result ) {
    printf( "test_add_as_encode: Result was not successful\n" );
  } else if ( optional_result.value() != 4 ) {
    printf( "test_add_as_encode: Result did not match: %d != 4\n", optional_result.value() );
  }
}

void test_curry_self( Runtime& rt )
{
  Handle curried
    = curry( rt, compile( file( "applications-prefix/src/applications-build/curry/curry.wasm" ) ), Handle( 2 ) );

  Handle curried_curry_result
    = apply_args( rt, curried, { compile( file( "testing/wasm-examples/add-simple.wasm" ) ), Handle( 2 ) } );

  Handle unwrapped_curried_curry_result = rt.storage().get_tree( curried_curry_result )[0];
  Handle curried_add_result = apply_args( rt, unwrapped_curried_curry_result, { Handle( 1 ), Handle( 3 ) } );

  auto optional_result = unwrap_tag( rt, curried_add_result );
  if ( not optional_result ) {
    printf( "test_curry_self: Result was not successful\n" );
  } else if ( optional_result.value() != 4 ) {
    printf( "test_curry_self: Result did not match: %d != 4\n", optional_result.value() );
  }
}

void test( void )
{
  auto& rt = Runtime::get_instance();
  rt.storage().deserialize();

  test_add_simple( rt );
  test_add_as_encode( rt );
  test_curry_self( rt );
}
