#include <stdio.h>

#include "relater.hh"
#include "test.hh"

namespace tester {
auto rt = std::make_shared<Relater>();
auto Limits = []() { return limits( *rt, 1024 * 1024 * 1024, 1024 * 1024, 1024 ); };
auto Compile = []( Handle<Fix> wasm ) { return compile( *rt, wasm ); };
auto File = []( std::filesystem::path path ) { return file( *rt, path ); };
auto Tree = []( auto... args ) { return handle::upcast( tree( *rt, args... ) ); };
auto Blob = []( std::string_view contents ) { return blob( *rt, contents ); };
}

using namespace std;

Handle<Fix> dirent( string_view name, string_view permissions, Handle<Fix> content )
{
  return tester::Tree( tester::Blob( name ), tester::Blob( permissions ), content );
}

Handle<Fix> fs()
{
  static string d = "040000";
  static string f = "100644";
  static auto e
    = dirent( ".", d, tester::Tree( dirent( "main.py", f, tester::Blob( "print(2 + 2)" ) ) ) );
  return e;
}

void test( void )
{
  auto python_exec = tester::Compile(
    tester::File( "applications-prefix/src/applications-build/flatware/examples/python/python-fixpoint.wasm" ) );
  auto input = flatware_input( *tester::rt,
                               tester::Limits(),
                               python_exec,
                               fs(),
                               tester::Tree( tester::Blob( "python" ), tester::Blob( "main.py" ) ) );
  auto result_handle = tester::rt->execute( input );
  auto result_tree = tester::rt->get( result_handle.try_into<ValueTree>().value() ).value();

  uint32_t x = -1;
  memcpy( &x, handle::extract<Literal>( result_tree->at( 0 ) ).value().data(), sizeof( uint32_t ) );
  if ( x != 0 ) {
    fprintf( stderr, "Return failed, returned %d != 0\n", x );
    exit( 1 );
  }

  auto out = handle::extract<Literal>( result_tree->at( 2 ) );
  if ( not out.has_value() ) {
    fprintf( stderr, "Output failed\n" );
    exit( 1 );
  }
  auto out_string = std::string_view( out.value().data(), out.value().size() );
  if ( out_string != "4\n" ) {
    fprintf( stderr, "Output failed, returned %s != 4\n", out_string.data() );
    exit( 1 );
  }
}
