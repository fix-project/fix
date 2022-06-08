#include <iostream>
#include <string>

#include "add.hh"
#include "mmap.hh"
#include "timer.hh"

using namespace std;

void baseline_function();
void static_add();
void virtual_add();
void char_add();
void virtual_char_add();
extern int add( int, int );

static int N;

int main( int argc, char* argv[] )
{
  if ( argc < 2 ) {
    cerr << "Usage: " << argv[0] << " #_of_iterations\n";
  }

  N = atoi( argv[1] );

  baseline_function();
  static_add();
  virtual_add();
  char_add();
  virtual_char_add();
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

void static_add()
{
  for ( int i = 0; i < N; i++ ) {
    GlobalScopeTimer<Timer::Category::Execution> record_timer;
    add( i, 42 );
  }

  global_timer().summary( cout );
  reset_global_timer();
}

void virtual_add()
{
  for ( int i = 0; i < N; i++ ) {
    Base* ptr = new Derived( i, 42 );
    {
      GlobalScopeTimer<Timer::Category::Execution> record_timer;
      ptr->add( i, 42 );
    }
    delete ptr;
  }

  global_timer().summary( cout );
  reset_global_timer();
}

void char_add()
{
  for ( int i = 0; i < N; i++ ) {
    uint32_t x = i;
    uint32_t y = 42;
    {
      GlobalScopeTimer<Timer::Category::Execution> record_timer;
      add( (char*)&x, (char*)&y );
    }
  }

  global_timer().summary( cout );
  reset_global_timer();
}

void virtual_char_add()
{
  for ( int i = 0; i < N; i++ ) {
    uint32_t x = i;
    uint32_t y = 42;
    Base* ptr = new Derived( i, 42 );
    {
      GlobalScopeTimer<Timer::Category::Execution> record_timer;
      ptr->add_char( (char*)&x, (char*)&y );
    }
  }

  global_timer().summary( cout );
  reset_global_timer();
}
