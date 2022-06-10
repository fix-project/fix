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

int N;
char* add_program_name;
char* add_cycle_program_name;
Name add_fixpoint_function;
Name add_wasi_function;
Name blob_42;
Name char_42;

#define ADD_TEST( func, message )                                                                                  \
  {                                                                                                                \
    cout << endl;                                                                                                  \
    cout << message << endl;                                                                                       \
    for ( int i = 0; i < N; i++ ) {                                                                                \
      func( i );                                                                                                   \
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
void char_add( int );
void virtual_char_add( int );
void vfork_add( int );
void vfork_add_rr();
void add_fixpoint( int );
void add_wasi( int );

int main( int argc, char* argv[] )
{
  if ( argc < 6 ) {
    cerr
      << "Usage: " << argv[0]
      << " #_of_iterations path_to_add_program path_to_add_cycle_program path_to_add_fixpoint path_to_add_wasi\n";
  }

  N = atoi( argv[1] );
  add_program_name = argv[2];
  add_cycle_program_name = argv[3];

  auto& runtime = RuntimeStorage::get_instance();
  ReadOnlyFile add_fixpoint_content { argv[4] };
  ReadOnlyFile add_wasi_content { argv[5] };
  add_fixpoint_function = runtime.add_blob( string_view( add_fixpoint_content ) );
  add_wasi_function = runtime.add_blob( string_view( add_wasi_content ) );

  blob_42 = runtime.add_blob( make_blob( 42 ) );
  const char* arg_42 = "42";
  char_42 = runtime.add_blob( string_view( arg_42, strlen( arg_42 ) + 1 ) );

  baseline_function();

  ADD_TEST( static_add, "Executing add statically linked..." );
  ADD_TEST( virtual_add, "Executing add via virtual function call..." );
  ADD_TEST( char_add, "Executing statically linked add with atoi..." );
  ADD_TEST( virtual_char_add, "Executing virtual add with atoi..." );
  ADD_TEST( vfork_add, "Executing add program..." );
  ADD_TEST_NUM( vfork_add_rr, "Executing add program rr...", 1 );
  ADD_TEST( add_fixpoint, "Executing add implemented in Fixpoint..." );
  ADD_TEST( add_wasi, "Executing add program compiled through wasi..." );
  return 0;
}

void baseline_function()
{
  for ( int i = 0; i < N; i++ ) {
    GlobalScopeTimer<Timer::Category::Execution> record_timer;
  }
  set_time_baseline( global_timer().mean<Timer::Category::Execution>() );
  reset_global_timer();
}

void static_add( int i )
{
  GlobalScopeTimer<Timer::Category::Execution> record_timer;
  add( i, 42 );
}

void virtual_add( int i )
{
  Base* ptr = new Derived( i, 42 );
  {
    GlobalScopeTimer<Timer::Category::Execution> record_timer;
    ptr->add( i, 42 );
  }
  delete ptr;
}

void char_add( int i )
{
  char const* x = "42";
  char const* y = to_string( i ).c_str();
  {
    GlobalScopeTimer<Timer::Category::Execution> record_timer;
    add( x, y );
  }
}

void virtual_char_add( int i )
{
  char const* x = "42";
  char const* y = to_string( i ).c_str();
  Base* ptr = new Derived( i, 42 );
  {
    GlobalScopeTimer<Timer::Category::Execution> record_timer;
    ptr->add_char( x, y );
  }
}

void vfork_add( int i )
{
  char const* arg1 = "42";
  char const* arg2 = to_string( i ).c_str();
  GlobalScopeTimer<Timer::Category::Execution> record_timer;
  pid_t pid = vfork();
  int wstatus;
  if ( pid == 0 ) {
    execl( add_program_name, "add", arg1, arg2, NULL );
  } else {
    waitpid( pid, &wstatus, 0 );
  }
}

void vfork_add_rr()
{
  char const* sudo = "/usr/bin/sudo";
  char const* rr = "rr";
  char const* record = "record";
  char const* N_char = to_string( N ).c_str();
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
  auto& runtime = RuntimeStorage::get_instance();
  Name arg2 = runtime.add_blob( make_blob( i ) );

  vector<Name> encode;
  encode.push_back( Name( "empty" ) );
  encode.push_back( add_fixpoint_function );
  encode.push_back( blob_42 );
  encode.push_back( arg2 );

  Name encode_name = runtime.add_tree( span_view<Name>( encode.data(), encode.size() ) );

  Thunk thunk( encode_name );
  Name thunk_name = runtime.add_thunk( thunk );
  runtime.force_thunk( thunk_name );
}

void add_wasi( int i )
{
  auto& runtime = RuntimeStorage::get_instance();
  const char* arg1_content = "add";
  const char* arg2_content = to_string( i ).c_str();
  Name arg1 = runtime.add_blob( string_view( arg1_content, strlen( arg1_content ) + 1 ) );
  Name arg2 = runtime.add_blob( string_view( arg2_content, strlen( arg2_content ) + 1 ) );

  vector<Name> encode;
  encode.push_back( Name( "empty" ) );
  encode.push_back( add_wasi_function );
  encode.push_back( arg1 );
  encode.push_back( char_42 );
  encode.push_back( arg2 );

  Name encode_name = runtime.add_tree( span_view<Name>( encode.data(), encode.size() ) );

  Thunk thunk( encode_name );
  Name thunk_name = runtime.add_thunk( thunk );
  runtime.force_thunk( thunk_name );
}
