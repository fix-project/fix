#include <stdio.h>

#include "relater.hh"
#include "test.hh"

namespace tester {
auto rt = std::make_shared<Relater>();
auto Limits = []() { return limits( *rt, 1024 * 1024, 1024, 1 ); };
auto Blob = []( std::string_view contents ) { return blob( *rt, contents ); };
auto Compile = []( Handle<Fix> wasm ) { return compile( *rt, wasm ); };
auto File = []( std::filesystem::path path ) { return file( *rt, path ); };
auto Tree = []( auto... args ) { return handle::upcast( tree( *rt, args... ) ); };
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
  static auto e = dirent(
    ".",
    d,
    tester::Tree(
      dirent( "fixpoint.txt", f, tester::Blob( "" ) ) ) );

  return e;
}

int run_flatware( const string& name, Handle<Fix> elf, Handle<Fix> home )
{
  printf( "### TEST %s\n", name.c_str() );
  auto exe = flatware_input( *tester::rt, tester::Limits(), elf, home );
  auto result = tester::rt->get( tester::rt->execute( exe ).try_into<ValueTree>().value() ).value();
  uint32_t code = -1;
  memcpy( &code, handle::extract<Literal>( result->at( 0 ) ).value().data(), sizeof( uint32_t ) );
  printf( "%s returned %d\n", name.c_str(), code );
  if ( code != 0 ) {
    fprintf( stderr, "%s exited with non-zero status\n", name.c_str() );
    exit( 1 );
  }
  return code;
}

void test( void )
{
  run_flatware( "filesys",
                tester::Compile( tester::File(
                  "applications-prefix/src/applications-build/flatware/examples/filesys/filesys-fixpoint.wasm" ) ),
                fs() );
}
