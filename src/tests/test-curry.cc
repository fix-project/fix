#include <memory>
#include <stdio.h>

#include "relater.hh"
#include "test.hh"
#include "types.hh"

namespace tester {
auto rt = std::make_shared<Relater>();
auto Limits = []() { return limits( *rt, 1024 * 1024, 1024, 1 ); };
auto Blob = []( std::string_view contents ) { return blob( *rt, contents ); };
auto Compile = []( Handle<Fix> wasm ) { return compile( *rt, wasm ); };
auto File = []( std::filesystem::path path ) { return file( *rt, path ); };
auto Tree = []( auto... args ) { return handle::upcast( tree( *rt, args... ) ); };
}

using namespace std;

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

optional<uint32_t> unwrap_tag( Handle<Fix> tag )
{
  auto h = handle::extract<ExpressionTree>( tag )
             .transform( []( auto h ) -> Handle<AnyTree> { return h; } )
             .or_else( [&]() -> optional<Handle<AnyTree>> { return handle::extract<ObjectTree>( tag ); } )
             .or_else( [&]() -> optional<Handle<AnyTree>> { return handle::extract<ValueTree>( tag ); } );

  if ( !h.has_value() or !h->visit<bool>( []( auto h ) { return h.is_tag(); } ) ) {
    cout << "unwrap_tag: Expected tag, got non-tag " << tag << endl;
    return {};
  }

  auto tag_state = tester::rt->get( h.value() ).value()->at( 2 );
  if ( handle::extract<Literal>( tag_state ).value() != Handle<Literal>( static_cast<uint32_t>( true ) ) ) {
    cout << "unwrap_tag: Tag was not successful " << tag_state << endl;
    return {};
  }

  auto tag_contents = tester::rt->get( h.value() ).value()->at( 1 );
  uint32_t blob_result = -1;
  memcpy( &blob_result, handle::extract<Literal>( tag_contents ).value().data(), sizeof( uint32_t ) );
  return blob_result;
}

void test_add_simple()
{
  auto curried = curry( add_simple_compiled, Handle<Literal>( 2 ) );
  auto curried_result = apply_args( curried, { Handle<Literal>( 1 ), Handle<Literal>( 3 ) } );

  auto optional_result = unwrap_tag( curried_result );
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

  auto optional_result = unwrap_tag( curried_result );
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

  auto unwrapped_curried_curry_result
    = handle::extract<Value>(
        tester::rt->get( curried_curry_result.try_into<ValueTree>().value() ).value()->at( 1 ) )
        .value();
  auto curried_add_result
    = apply_args( unwrapped_curried_curry_result, { Handle<Literal>( 1 ), Handle<Literal>( 3 ) } );

  auto optional_result = unwrap_tag( curried_add_result );
  if ( not optional_result ) {
    printf( "test_curry_self: Result was not successful\n" );
    exit( 1 );
  } else if ( optional_result.value() != 4 ) {
    printf( "test_curry_self: Result did not match: %d != 4\n", optional_result.value() );
    exit( 1 );
  }
}

void test( void )
{
  curry_compiled = tester::Compile( tester::File( "applications-prefix/src/applications-build/curry/curry.wasm" ) );
  add_simple_compiled = tester::Compile( tester::File( "testing/wasm-examples/add-simple.wasm" ) );

  test_add_simple();
  test_add_as_encode();
  test_curry_self();
  exit( 0 );
}
