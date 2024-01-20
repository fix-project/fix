#include <stdio.h>

#include "handle_post.hh"
#include "test.hh"

namespace tester {
auto rt = ReadOnlyTester::init();
auto Blob = []( std::string_view contents ) { return blob( *rt, contents ); };
auto Compile = []( Handle<Fix> wasm ) { return compile( *rt, wasm ); };
auto File = []( std::filesystem::path path ) { return file( *rt, path ); };
auto Tree = []( auto... args ) { return handle::upcast( tree( *rt, args... ) ); };
}

using namespace std;

uint32_t fix_add( char a, char b, Handle<Fix> add_elf )
{
  auto add = Handle<Application>(
    tester::Tree( tester::Blob( "unused" ),
                  add_elf,
                  tester::Tree( tester::Blob( "add" ), tester::Blob( { &a, 1 } ), tester::Blob( { &b, 1 } ) ) ) );
  (void)a, (void)b;

  auto result = tester::rt->executor().execute( Handle<Eval>( add ) );
  auto tree = tester::rt->get( result.try_into<ValueTree>().value() ).value();
  auto sum = tree->at( 0 );
  uint32_t x = -1;
  memcpy( &x, handle::extract<Literal>( sum )->data(), sizeof( uint32_t ) );
  return x;
}

void check_add( uint8_t a, uint8_t b, Handle<Fix> add, string name )
{
  uint32_t sum_blob = fix_add( a, b, add );
  printf( "%s: %u + %u = %u\n", name.c_str(), (uint8_t)a, (uint8_t)b, sum_blob );
  if ( sum_blob != (uint32_t)a + (uint32_t)b ) {
    fprintf(
      stderr, "%s: got %u + %u = %u, expected %u.\n", name.c_str(), a, b, sum_blob, (uint32_t)a + (uint32_t)b );
    exit( 1 );
  }
}

void check_add( uint8_t a, uint8_t b )
{
  static Handle add_flatware = tester::Compile(
    tester::File( "applications-prefix/src/applications-build/flatware/examples/add/add-fixpoint.wasm" ) );
  check_add( a, b, add_flatware, "add-fixpoint" );
}

void test( void )
{
  for ( size_t i = 0; i < 32; i++ ) {
    check_add( random(), random() );
  }
  check_add( 0, 0 );
}
