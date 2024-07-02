#include <memory>

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

void test( shared_ptr<Relater> rt )
{
  tester::rt = rt;

  auto mapreduce
    = tester::Compile( tester::File( "applications-prefix/src/applications-build/mapreduce/mapreduce.wasm" ) );
  auto curry = tester::Compile( tester::File( "applications-prefix/src/applications-build/curry/curry.wasm" ) );
  auto sum = tester::Compile( tester::File( "testing/wasm-examples/add-simple.wasm" ) );
  auto curryingSum = Handle<Strict>( Handle<Thunk>( tester::Tree( tester::Limits(), curry, sum, 2_literal32 ) ) );
  auto add100 = Handle<Strict>( Handle<Thunk>( tester::Tree( tester::Limits(), curryingSum, 0x100_literal32 ) ) );
  auto original = tester::Tree( 1_literal32,
                                2_literal32,
                                3_literal32,
                                4_literal32,
                                5_literal32,
                                6_literal32,
                                7_literal32,
                                8_literal32,
                                9_literal32,
                                10_literal32,
                                11_literal32,
                                12_literal32,
                                13_literal32,
                                14_literal32,
                                15_literal32 );

  auto thunk = Handle<Thunk>(
    tester::Tree( tester::Limits(), mapreduce, add100, sum, original, tester::Limits(), tester::Limits() ) );
  auto result = tester::rt->execute( Handle<Eval>( thunk ) );

  cout << result << endl;

  exit( 0 );
}
