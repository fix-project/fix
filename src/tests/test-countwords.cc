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
  auto thunk = Handle<Thunk>( tester::Tree(
    tester::Limits(), mapreduce, count_words, merge_counts, tree, tester::Limits(), tester::Limits() ) );
  auto result = tester::rt->execute( Handle<Eval>( thunk ) );

  auto x = result.unwrap<Blob>().unwrap<Named>();
  auto y = tester::rt->get( x ).value();

  uint64_t actual_counts[256] = { 0 };
  for ( char c : tester::rt->get( blob.unwrap<Named>() ).value()->span() ) {
    actual_counts[static_cast<uint8_t>( c )]++;
  }
  for ( char c : tester::rt->get( blob2.unwrap<Named>() ).value()->span() ) {
    actual_counts[static_cast<uint8_t>( c )]++;
  }

  for ( size_t i = 0; i < 256; i++ ) {
    if ( actual_counts[i] != reinterpret_cast<const uint64_t*>( y->span().data() )[i] ) {
      fprintf( stderr,
               "%ld: got %ld, expected %ld.\n",
               i,
               reinterpret_cast<const uint64_t*>( y->span().data() )[i],
               actual_counts[i] );
      exit( 1 );
    }
  }

  exit( 0 );
}
