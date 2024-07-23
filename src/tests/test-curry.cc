#include <memory>
#include <stdio.h>

#include "relater.hh"
#include "test.hh"
#include "types.hh"

using namespace std;

namespace tester {
shared_ptr<Relater> rt;
auto Limits = []() { return limits( *rt, 1024 * 1024, 1024, 1 ); };
auto Blob = []( std::string_view contents ) { return blob( *rt, contents ); };
auto Compile = []( Handle<Fix> wasm ) { return compile( *rt, wasm ); };
auto File = []( std::filesystem::path path ) { return file( *rt, path ); };
auto Tree = []( auto... args ) { return handle::upcast( tree( *rt, args... ) ); };
}

static Handle<Fix> curry_compiled;
static Handle<Fix> add_simple_compiled;

Handle<Value> curry( Handle<Fix> program, Handle<Fix> num_args )
{
  return tester::rt->execute(
    Handle<Eval>( Handle<Application>( tester::Tree( tester::Limits(), curry_compiled, program, num_args ) ) ) );
}

Handle<Value> apply_args( Handle<Value> curried, const initializer_list<Handle<Fix>>& elements )
{
  for ( auto& e : elements ) {
    curried
      = tester::rt->execute( Handle<Eval>( Handle<Application>( tester::Tree( tester::Limits(), curried, e ) ) ) );
  }
  return curried;
}

optional<uint32_t> unwrap_result( Handle<Fix> result )
{
  return handle::extract<Literal>( result ).transform( []( auto x ) { return uint32_t( x ); } );
}

void test_add_simple()
{
  auto curried = curry( add_simple_compiled, Handle<Literal>( 2 ) );
  auto curried_result = apply_args( curried, { Handle<Literal>( 1 ), Handle<Literal>( 3 ) } );

  auto optional_result = unwrap_result( curried_result );
  if ( not optional_result ) {
    printf( "test_add_simple: Result was not successful\n" );
    exit( 1 );
  } else if ( optional_result.value() != 4 ) {
    printf( "test_add_simple: Result did not match: %d != 4\n", optional_result.value() );
    exit( 1 );
  }
}

void test_add_as_encode()
{
  auto curried = curry( tester::Tree( tester::Limits(), add_simple_compiled ), Handle<Literal>( 2 ) );

  auto curried_result = apply_args( curried, { Handle<Literal>( 1 ), Handle<Literal>( 3 ) } );

  auto optional_result = unwrap_result( curried_result );
  if ( not optional_result ) {
    printf( "test_add_as_encode: Result was not successful\n" );
    exit( 1 );
  } else if ( optional_result.value() != 4 ) {
    printf( "test_add_as_encode: Result did not match: %d != 4\n", optional_result.value() );
    exit( 1 );
  }
}

void test_curry_self()
{
  auto curried = curry( curry_compiled, Handle<Literal>( 2 ) );

  auto curried_curry_result = apply_args( curried, { add_simple_compiled, Handle<Literal>( 2 ) } );

  auto curried_add_result = apply_args( curried_curry_result, { Handle<Literal>( 1 ), Handle<Literal>( 3 ) } );

  auto optional_result = unwrap_result( curried_add_result );
  if ( not optional_result ) {
    printf( "test_curry_self: Result was not successful\n" );
    exit( 1 );
  } else if ( optional_result.value() != 4 ) {
    printf( "test_curry_self: Result did not match: %d != 4\n", optional_result.value() );
    exit( 1 );
  }
}

void test( shared_ptr<Relater> rt )
{
  tester::rt = rt;

  curry_compiled = tester::Compile( tester::File( "applications-prefix/src/applications-build/curry/curry.wasm" ) );
  add_simple_compiled = tester::Compile( tester::File( "testing/wasm-examples/add-simple.wasm" ) );

  test_add_simple();
  test_add_as_encode();
  test_curry_self();
  exit( 0 );
}
