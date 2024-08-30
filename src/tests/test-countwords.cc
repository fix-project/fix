#include <memory>
#include <stdio.h>

#include "relater.hh"
#include "test.hh"
#include "types.hh"

using namespace std;

namespace tester {
shared_ptr<Relater> rt;
auto Limits = []() { return limits( *rt, 1024 * 1024 * 1024, 1024, 1 ); };
auto Blob = []( std::string_view contents ) { return blob( *rt, contents ); };
auto Compile = []( Handle<Fix> wasm ) { return compile( *rt, wasm ); };
auto File = []( std::filesystem::path path ) { return file( *rt, path ); };
auto Tree = []( auto... args ) { return handle::upcast( tree( *rt, args... ) ); };
}

void test( shared_ptr<Relater> rt )
{
  tester::rt = rt;

  auto mapreduce
    = tester::Compile( tester::File( "applications-prefix/src/applications-build/mapreduce/mapreduce.wasm" ) );
  auto count_words
    = tester::Compile( tester::File( "applications-prefix/src/applications-build/count-words/count_words.wasm" ) );
  auto merge_counts
    = tester::Compile( tester::File( "applications-prefix/src/applications-build/count-words/merge_counts.wasm" ) );

  auto goal = tester::Blob( "the" );
  auto blob0 = tester::Blob( "the quick brown fox jumps over the lazy dog" );
  auto blob1 = tester::Blob( "it was the best of times, it was the worst of times" );

  auto arg0 = tester::Tree( goal, blob0 );
  auto arg1 = tester::Tree( goal, blob1 );

  auto tree = tester::Tree( arg0, arg1 );
  auto thunk = Handle<Thunk>( tester::Tree(
    tester::Limits(), mapreduce, count_words, merge_counts, tree, tester::Limits(), tester::Limits() ) );
  auto result = tester::rt->execute( Handle<Eval>( thunk ) );

  auto x = uint64_t( result.unwrap<Blob>().unwrap<Literal>() );
  uint64_t y = 4;

  if ( x != y ) {
    fprintf( stderr, "got %ld, expected %ld.\n", x, y );
    exit( 1 );
  }

  exit( 0 );
}
