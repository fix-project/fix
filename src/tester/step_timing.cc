#include <iostream>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "mmap.hh"
#include "name.hh"
#include "runtimestorage.hh"
#include "timer.hh"

#define INIT_INSTANCE 4096

using namespace std;

struct addend
{
  char a[2];
  char b[2];
};

addend arguments[INIT_INSTANCE];

char* add_program_name;
char* add_cycle_program_name;

auto& runtime = RuntimeStorage::get_instance();
Name add_fixpoint_thunk[INIT_INSTANCE];
Name add_wasi_thunk[INIT_INSTANCE];

addend make_addend( int i )
{
  addend result;
  result.a[0] = static_cast<char>( i / 255 + 1 );
  result.b[0] = static_cast<char>( i % 255 + 1 );
  result.a[1] = static_cast<char>( 0 );
  result.b[1] = static_cast<char>( 0 );
  return result;
}

int main( int argc, char* argv[] )
{
  if ( argc < 3 ) {
    cerr << "Usage: " << argv[0] << " path_to_add_fixpoint path_to_add_wasi\n";
  }

  ReadOnlyFile add_fixpoint_content { argv[1] };
  ReadOnlyFile add_wasi_content { argv[2] };

  cout << "Compiling and populating add in fixpoint..." << endl;
  Name add_fixpoint_function = runtime.add_blob( string_view( add_fixpoint_content ) );
  runtime.populate_program( add_fixpoint_function );
  global_timer().summary( cout );
  reset_global_timer();

  cout << "Compiling and populating add in wasi..." << endl;
  Name add_wasi_function = runtime.add_blob( string_view( add_wasi_content ) );
  runtime.populate_program( add_wasi_function );
  global_timer().summary( cout );
  reset_global_timer();

  const char* add_content = "add";
  Name add_char = runtime.add_blob( string_view( add_content, strlen( add_content ) + 1 ) );

  // Populate addends
  for ( int i = 0; i < INIT_INSTANCE; i++ ) {
    arguments[i] = make_addend( i );
  }

  // Populate encodes
  for ( int i = 0; i < INIT_INSTANCE; i++ ) {
    Name arg1 = runtime.add_blob( make_blob( string_view( arguments[i].a, 2 ) ) );
    Name arg2 = runtime.add_blob( make_blob( string_view( arguments[i].b, 2 ) ) );

    Tree_ptr fix_encode { static_cast<Name*>( aligned_alloc( alignof( Name ), sizeof( Name ) * 4 ) ) };
    if ( not fix_encode ) {
      throw bad_alloc();
    }
    Tree_ptr wasi_encode { static_cast<Name*>( aligned_alloc( alignof( Name ), sizeof( Name ) * 5 ) ) };
    if ( not wasi_encode ) {
      throw bad_alloc();
    }

    fix_encode.get()[0] = Name( "empty" );
    fix_encode.get()[1] = add_fixpoint_function;
    fix_encode.get()[2] = arg1;
    fix_encode.get()[3] = arg2;

    wasi_encode.get()[0] = Name( "empty" );
    wasi_encode.get()[1] = add_wasi_function;
    wasi_encode.get()[2] = add_char;
    wasi_encode.get()[3] = arg1;
    wasi_encode.get()[4] = arg2;

    Name fix_encode_name = runtime.add_tree( { move( fix_encode ), 4 } );
    Name wasi_encode_name = runtime.add_tree( { move( wasi_encode ), 5 } );

    Thunk fix_thunk( fix_encode_name );
    Name fix_thunk_name = runtime.add_thunk( fix_thunk );
    Thunk wasi_thunk( wasi_encode_name );
    Name wasi_thunk_name = runtime.add_thunk( wasi_thunk );

    add_fixpoint_thunk[i] = fix_thunk_name;
    add_wasi_thunk[i] = wasi_thunk_name;
  }

  return 0;
}
