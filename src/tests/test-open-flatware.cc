#include <stdio.h>

#include "test.hh"

using namespace std;

Handle dirent( string_view name, string_view permissions, Handle content )
{
  return tree( { blob( name ), blob( permissions ), content } );
}

Handle fs()
{
  static string d = "040000";
  static string f = "100644";
  static Handle e = dirent(
    ".",
    d,
    tree(
      { dirent(
          "a",
          d,
          tree(
            { dirent( "a_1.txt", f, blob( "Hello, this is a_1.txt!" ) ),
              dirent( "a_2.txt", f, blob( "Hello, this is a_2.txt!" ) ),
              dirent(
                "b",
                d,
                tree( { dirent( "b_1.txt", f, blob( "this is b_1.txt!" ) ),
                        dirent( "hello",
                                d,
                                tree( {
                                  dirent( "greeter.txt", f, blob( "Hi, I am greeter.txt" ) ),
                                } ) ),
                        dirent( "c", d, tree( { dirent( "fixpoint", f, blob( "Hello, World!" ) ) } ) ) } ) ) } ) ),
        dirent( "fixpoint", f, blob( "Hello, World!" ) ) } ) );
  return e;
}

int run_flatware( const string& name, Handle elf, Handle home )
{
  printf( "### TEST %s\n", name.c_str() );
  Handle exe = flatware_input( elf, home );
  auto& rt = Runtime::get_instance();
  Handle result_handle = rt.eval( exe );
  Tree result = rt.storage().get_tree( result_handle );
  uint32_t code = -1;
  memcpy( &code, result[0].literal_blob().data(), sizeof( uint32_t ) );
  printf( "%s returned %d\n", name.c_str(), code );
  if ( code != 0 ) {
    fprintf( stderr, "%s exited with non-zero status\n", name.c_str() );
    exit( 1 );
  }
  return code;
}

void test( void )
{
  auto& rt = Runtime::get_instance();
  rt.storage().deserialize();
  run_flatware(
    "open",
    compile( file( "applications-prefix/src/applications-build/flatware/examples/open/open-fixpoint.wasm" ) ),
    fs() );
  run_flatware(
    "open-deep",
    compile( file( "applications-prefix/src/applications-build/flatware/examples/open/open-deep-fixpoint.wasm" ) ),
    fs() );
}
