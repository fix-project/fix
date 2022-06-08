#include <iostream>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "add.hh"
#include "mmap.hh"
#include "timer.hh"

using namespace std;

static int N;
static char* add_program_name;

#define ADD_TEST( func )                                                                                           \
  {                                                                                                                \
    for ( int i = 0; i < N; i++ ) {                                                                                \
      func( i );                                                                                                   \
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
extern int add( int, int );

int main( int argc, char* argv[] )
{
  if ( argc < 3 ) {
    cerr << "Usage: " << argv[0] << " #_of_iterations path_to_add_program\n";
  }

  N = atoi( argv[1] );
  add_program_name = argv[2];

  baseline_function();

  ADD_TEST( static_add );
  ADD_TEST( virtual_add );
  ADD_TEST( char_add );
  ADD_TEST( virtual_char_add );
  ADD_TEST( vfork_add );
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

inline void vfork_add( int i )
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
