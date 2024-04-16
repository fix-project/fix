#include <memory>
#include <stdio.h>

#include "relater.hh"
#include "test.hh"
#include "types.hh"

namespace tester {
auto rt = std::make_shared<Relater>();
auto Limits = []() { return limits( *rt, 1024 * 1024 * 1024, 1024, 1 ); };
auto Blob = []( std::string_view contents ) { return blob( *rt, contents ); };
auto Compile = []( Handle<Fix> wasm ) { return compile( *rt, wasm ); };
auto File = []( std::filesystem::path path ) { return file( *rt, path ); };
auto Tree = []( auto... args ) { return handle::upcast( tree( *rt, args... ) ); };
}

using namespace std;

void test( void )
{
  auto count_words
    = tester::Compile( tester::File( "applications-prefix/src/applications-build/count-words/count_words.wasm" ) );
  /* auto count_words = tester::Compile( tester::File( "testing/wasm-examples/add-simple.wasm" ) ); */
  /* auto blob = tester::Blob( "hello world" ); */
  auto thunk = Handle<Thunk>( tester::Tree( tester::Limits(), count_words, 123_literal32, 234_literal32 ) );
  cout << thunk << endl;
  auto result = tester::rt->execute( Handle<Eval>( thunk ) );

  cout << result << endl;

  exit( 0 );
}
