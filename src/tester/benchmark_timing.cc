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

using namespace std;

struct addend
{
  char a[2];
  char b[2];
};

Base* base_objects[INIT_INSTANCE];
addend arguments[INIT_INSTANCE];

char* add_program_name;
char* add_cycle_program_name;

auto& runtime = RuntimeStorage::get_instance();
Name add_fixpoint_thunk[INIT_INSTANCE];
Name add_wasi_thunk[INIT_INSTANCE];

#define ADD_TEST( func, message )                                                                                  \
  {                                                                                                                \
    cout << endl;                                                                                                  \
    cout << message << endl;                                                                                       \
    {                                                                                                              \
      GlobalScopeTimer<Timer::Category::Execution> record_timer;                                                   \
      for ( int i = 0; i < INIT_INSTANCE; i++ ) {                                                                  \
        func( i );                                                                                                 \
      }                                                                                                            \
    }                                                                                                              \
    global_timer().summary( cout );                                                                                \
    reset_global_timer();                                                                                          \
  }

#define ADD_TEST_NUM( func, message, cycle )                                                                       \
  {                                                                                                                \
    cout << endl;                                                                                                  \
    cout << message << endl;                                                                                       \
    for ( int i = 0; i < cycle; i++ ) {                                                                            \
      func();                                                                                                      \
    }                                                                                                              \
    global_timer().summary( cout );                                                                                \
    reset_global_timer();                                                                                          \
  }

void baseline_function();
void static_add( int );
void virtual_add( int );
void vfork_add( int );
void vfork_add_rr();
void add_fixpoint( int );
void add_wasi( int );

addend make_addend( int i )
{
  addend result;
  result.a[0] = static_cast<char>( i / 254 + 1 );
  result.b[0] = static_cast<char>( i % 254 + 1 );
  result.a[1] = static_cast<char>( 0 );
  result.b[1] = static_cast<char>( 0 );
  return result;
}

int main( int argc, char* argv[] )
{
  if ( argc < 5 ) {
    cerr << "Usage: " << argv[0]
         << " path_to_add_program path_to_add_cycle_program path_to_add_fixpoint path_to_add_wasi\n";
  }

  add_program_name = argv[1];
  add_cycle_program_name = argv[2];

  ReadOnlyFile add_fixpoint_content { argv[3] };
  ReadOnlyFile add_wasi_content { argv[4] };
  Name add_fixpoint_function = runtime.add_blob( string_view( add_fixpoint_content ) );
  Name add_wasi_function = runtime.add_blob( string_view( add_wasi_content ) );
  runtime.populate_program( add_fixpoint_function );
  runtime.populate_program( add_wasi_function );

  const char* add_content = "add";
  Name add_char = runtime.add_blob( string_view( add_content, strlen( add_content ) + 1 ) );

#if TIME_FIXPOINT == 1
  baseline_function();
#endif

  // Populate addends
  for ( int i = 0; i < INIT_INSTANCE; i++ ) {
    arguments[i] = make_addend( i );
  }

  // Populate derived objects
  for ( int i = 0; i < INIT_INSTANCE; i++ ) {
    base_objects[i] = make_derived();
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

  ADD_TEST( static_add, "Executing add statically linked..." );
  ADD_TEST( virtual_add, "Executing add via virtual function call..." );
  ADD_TEST( add_fixpoint, "Executing add implemented in Fixpoint..." );
  ADD_TEST( add_wasi, "Executing add program compiled through wasi..." );
  ADD_TEST( vfork_add, "Executing add program..." );
  ADD_TEST_NUM( vfork_add_rr, "Executing add program rr...", 1 );

  for ( int i = 0; i < INIT_INSTANCE; i++ ) {
    delete ( base_objects[i] );
  }

  return 0;
}

void baseline_function()
{
  for ( int i = 0; i < INIT_INSTANCE; i++ ) {
    GlobalScopeTimer<Timer::Category::Execution> record_timer;
  }
  set_time_baseline( global_timer().mean<Timer::Category::Execution>() );
  reset_global_timer();
}

void static_add( int i )
{
  add( arguments[i].a, arguments[i].b );
}

void virtual_add( int i )
{
  base_objects[i]->add( arguments[i].a, arguments[i].b );
}

void vfork_add( int i )
{
  pid_t pid = vfork();
  int wstatus;
  if ( pid == 0 ) {
    execl( add_program_name, "add", arguments[i].a, arguments[i].b, NULL );
  } else {
    waitpid( pid, &wstatus, 0 );
  }
}

void vfork_add_rr()
{
  char const* sudo = "/usr/bin/sudo";
  char const* rr = "rr";
  char const* record = "record";
  char const* N_char = to_string( INIT_INSTANCE ).c_str();
  {
    GlobalScopeTimer<Timer::Category::Execution> record_timer;
    pid_t pid = vfork();
    int wstatus;
    if ( pid == 0 ) {
      execl( sudo, sudo, rr, record, add_cycle_program_name, add_program_name, N_char, NULL );
    } else {
      waitpid( pid, &wstatus, 0 );
    }
  }
}

void add_fixpoint( int i )
{
  runtime.force_thunk( add_fixpoint_thunk[i] );
}

void add_wasi( int i )
{
  runtime.force_thunk( add_wasi_thunk[i] );
}
