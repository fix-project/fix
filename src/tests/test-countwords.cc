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
  auto mapreduce
    = tester::Compile( tester::File( "applications-prefix/src/applications-build/mapreduce/mapreduce.wasm" ) );
  auto count_words
    = tester::Compile( tester::File( "applications-prefix/src/applications-build/count-words/count_words.wasm" ) );
  auto merge_counts
    = tester::Compile( tester::File( "applications-prefix/src/applications-build/count-words/merge_counts.wasm" ) );

  auto blob = tester::Blob( "the quick brown fox jumps over the lazy dog" );
  auto blob2 = tester::Blob( "it was the best of times, it was the worst of times" );
  auto tree = tester::Tree( blob, blob2 );
  auto thunk = Handle<Thunk>( tester::Tree( tester::Limits(), mapreduce, count_words, merge_counts, tree ) );
  auto result = tester::rt->execute( Handle<Eval>( thunk ) );

  auto x = result.unwrap<Blob>().unwrap<Named>();
  auto y = tester::rt->get( x ).value();
  auto actual = std::string( y->span().data(), y->span().size() );

  std::string expected = "best,0x00000001\n"
                         "brown,0x00000001\n"
                         "dog,0x00000001\n"
                         "fox,0x00000001\n"
                         "it,0x00000002\n"
                         "jumps,0x00000001\n"
                         "lazy,0x00000001\n"
                         "of,0x00000002\n"
                         "over,0x00000001\n"
                         "quick,0x00000001\n"
                         "the,0x00000004\n"
                         "times,0x00000002\n"
                         "was,0x00000002\n"
                         "worst,0x00000001\n";

  CHECK_EQ( expected, actual );

  exit( 0 );
}
