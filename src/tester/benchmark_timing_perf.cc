#include <iostream>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "add.hh"
#include "mmap.hh"
#include "name.hh"
#include "runtimestorage.hh"
#include "timer.hh"

#define INIT_INSTANCE 2601000

using namespace std;

struct addend
{
  char a[3];
  char b[3];
};

auto& runtime = RuntimeStorage::get_instance();
Name add_fixpoint_thunk[INIT_INSTANCE];
addend arguments[INIT_INSTANCE];

addend make_addend( int i )
{
  addend result;
  result.a[0] = static_cast<char>( i / 255 + 1 );
  result.b[0] = static_cast<char>( i % 255 + 1 );
  result.a[1] = static_cast<char>( i / 65025 + 1 );
  result.b[1] = static_cast<char>( i / 65025 + 1 );
  result.a[2] = static_cast<char>( 0 );
  result.b[2] = static_cast<char>( 0 );
  return result;
}

int main( int argc, char* argv[] )
{
  if ( argc < 2 ) {
    cerr << "Usage: " << argv[0] << " path_to_add_fixpoint\n";
  }
  global_timer().start<Timer::Category::Execution>();
  runtime.set_init_instances( INIT_INSTANCE );

  ReadOnlyFile add_fixpoint_content { argv[1] };
  Name add_fixpoint_function = runtime.add_blob( string_view( add_fixpoint_content ) );
  runtime.populate_program( add_fixpoint_function );

  // Populate addends
  for ( int i = 0; i < INIT_INSTANCE; i++ ) {
    arguments[i] = make_addend( i );
  }

  // Populate encodes
  for ( int i = 0; i < INIT_INSTANCE; i++ ) {
    Name arg1 = runtime.add_blob( make_blob( string_view( arguments[i].a, 3 ) ) );
    Name arg2 = runtime.add_blob( make_blob( string_view( arguments[i].b, 3 ) ) );

    Tree_ptr fix_encode { static_cast<Name*>( aligned_alloc( alignof( Name ), sizeof( Name ) * 4 ) ) };
    if ( not fix_encode ) {
      throw bad_alloc();
    }

    fix_encode.get()[0] = Name( "empty" );
    fix_encode.get()[1] = add_fixpoint_function;
    fix_encode.get()[2] = arg1;
    fix_encode.get()[3] = arg2;

    Name fix_encode_name = runtime.add_tree( { move( fix_encode ), 4 } );

    Thunk fix_thunk( fix_encode_name );
    Name fix_thunk_name = runtime.add_thunk( fix_thunk );

    add_fixpoint_thunk[i] = fix_thunk_name;
  }
  global_timer().stop<Timer::Category::Execution>();
  global_timer().summary( cout );
  reset_global_timer();

  int pid = getpid();
  int cpid = fork();
  if ( cpid == 0 ) {
    const char* pid_char = to_string( pid ).c_str();
    execl("/usr/bin/sudo", "sudo", "perf", "record", "-p", pid_char, NULL);
    // execl( "/usr/bin/sudo",
    //        "sudo",
    //        "perf",
    //        "record",
    //        "--call-graph",
    //        "dwarf",
    //        "-e",
    //        "cpu-clock,faults",
    //        "-p",
    //        pid_char,
    //        NULL );
  } else {
    {
      GlobalScopeTimer<Timer::Category::Execution> record_timer;
      for ( int i = 0; i < INIT_INSTANCE; i++ ) {
        runtime.force_thunk( add_fixpoint_thunk[i] );
      }
    }
    global_timer().summary( cout );
    reset_global_timer();

    kill( cpid, SIGINT );
    waitpid( cpid, nullptr, 0 );
  }

  return 0;
}
