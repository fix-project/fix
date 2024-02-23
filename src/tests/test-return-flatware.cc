#include <stdio.h>

#include "test.hh"

using namespace std;

namespace tester {
auto rt = ReadOnlyRT::init();
auto Blob = []( std::string_view contents ) { return blob( *rt, contents ); };
auto Compile = []( Handle<Fix> wasm ) { return compile( *rt, wasm ); };
auto File = []( std::filesystem::path path ) { return file( *rt, path ); };
auto Tree = []( auto... args ) { return handle::upcast( tree( *rt, args... ) ); };
}

void test( void )
{
  auto res = tester::rt->execute( flatware_input( *tester::rt, tester::Compile( tester::File(
      "applications-prefix/src/applications-build/flatware/examples/return3/return3-fixpoint.wasm" ) ) ) );
  auto result_tree = tester::rt->get( res.try_into<ValueTree>().value() ).value();
  auto blob = handle::extract<Literal>( result_tree->at( 0 ) ).value();
  CHECK( blob.size() == 4 );
  uint32_t x = *( reinterpret_cast<const uint32_t*>( blob.data() ) );
  if ( x != 3 ) {
    fprintf( stderr, "Return failed, returned %d != 3\n", x );
    exit( 1 );
  }
}
