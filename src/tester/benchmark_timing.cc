#include <iostream>
#include <string>

#include "add.hh"
#include "mmap.hh"
#include "timer.hh"

using namespace std;

static int N;

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
extern int add( int, int );

int main( int argc, char* argv[] )
{
  if ( argc < 2 ) {
    cerr << "Usage: " << argv[0] << " #_of_iterations\n";
  }

  N = atoi( argv[1] );
  baseline_function();

  ADD_TEST( static_add );
  ADD_TEST( virtual_add );
  ADD_TEST( char_add );
  ADD_TEST( virtual_char_add );
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
  uint32_t x = i;
  uint32_t y = 42;
  {
    GlobalScopeTimer<Timer::Category::Execution> record_timer;
    add( (char*)&x, (char*)&y );
  }
}

void virtual_char_add( int i )
{
  uint32_t x = i;
  uint32_t y = 42;
  Base* ptr = new Derived( i, 42 );
  {
    GlobalScopeTimer<Timer::Category::Execution> record_timer;
    ptr->add_char( (char*)&x, (char*)&y );
  }
}
