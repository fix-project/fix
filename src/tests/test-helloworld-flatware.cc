#include <stdio.h>

#include "handle_post.hh"
#include "test.hh"

namespace tester {
auto rt = ReadOnlyRT::init();
auto Blob = []( std::string_view contents ) { return blob( *rt, contents ); };
auto Compile = []( Handle<Fix> wasm ) { return compile( *rt, wasm ); };
auto File = []( std::filesystem::path path ) { return file( *rt, path ); };
auto Tree = []( auto... args ) { return handle::upcast( tree( *rt, args... ) ); };
}

using namespace std;

void test( void )
{

  auto input = flatware_input(
    *tester::rt,
    tester::Compile( tester::File(
      "applications-prefix/src/applications-build/flatware/examples/helloworld/helloworld-fixpoint.wasm" ) ) );
  auto result_handle = tester::rt->execute( input );
  auto result_tree = tester::rt->get( result_handle.try_into<ValueTree>().value() ).value();

  uint32_t x = -1;
  memcpy( &x, handle::extract<Literal>( result_tree->at( 0 ) ).value().data(), sizeof( uint32_t ) );
  if ( x != 0 ) {
    fprintf( stderr, "Return failed, returned %d != 0\n", x );
    exit( 1 );
  }

  auto out = handle::extract<Literal>( result_tree->at( 2 ) ).value();
  if ( std::string_view( out.data(), out.size() ) != "Hello, World!\n" ) {
    fprintf( stderr, "Output did not match expected 'Hello, World!'\n" );
    fprintf( stderr, "Output: %.*s", (int)out.size(), out.data() );
    exit( 1 );
  }
}
