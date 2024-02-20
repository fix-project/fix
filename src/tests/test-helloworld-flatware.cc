#include <stdio.h>

#include "relater.hh"
#include "test.hh"

namespace tester {
auto rt = std::make_shared<Relater>();
auto Blob = []( std::string_view contents ) { return blob( *rt, contents ); };
auto Compile = []( Handle<Fix> wasm ) { return compile( *rt, wasm ); };
auto File = []( std::filesystem::path path ) { return file( *rt, path ); };
auto Tree = []( auto... args ) { return handle::upcast( tree( *rt, args... ) ); };
}

using namespace std;

void test( void )
{
  auto res = tester::rt->execute( Handle<Eval>( Handle<Application>(
    tester::Tree( tester::Blob( "unused" ),
                  tester::Compile( tester::File( "applications-prefix/src/applications-build/flatware/examples/"
                                                 "helloworld/helloworld-fixpoint.wasm" ) ) ) ) ) );

  auto result_tree = tester::rt->get( res.try_into<ValueTree>().value() ).value();

  uint32_t x = -1;
  memcpy( &x, handle::extract<Literal>( result_tree->at( 0 ) ).value().data(), sizeof( uint32_t ) );
  if ( x != 0 ) {
    fprintf( stderr, "Return failed, returned %d != 0\n", x );
    exit( 1 );
  }

  auto out = handle::extract<Literal>( result_tree->at( 1 ) ).value();
  if ( std::string_view( out.data(), out.size() ) != "Hello, World!" ) {
    fprintf( stderr, "Output did not match expected 'Hello, World!'\n" );
    exit( 1 );
  }
}
