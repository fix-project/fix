#include <iostream>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

#include "add.hh"
#include "mmap.hh"
#include "relater.hh"
#include "scheduler.hh"
#include "test.hh"
#include "timer.hh"

#define INIT_INSTANCE 4096

using namespace std;

struct addend
{
  char a[2];
  char b[2];
};

Base* base_objects[INIT_INSTANCE];
addend arguments[INIT_INSTANCE + 1];

char* add_program_name;

static Handle<Application> compile_application( IRuntime& rt, Handle<Fix> wasm )
{
  auto compiler = rt.labeled( "compile-encode" );

  auto kibi = 1024;
  auto mebi = 1024 * kibi;
  auto gibi = 1024 * mebi;

  auto tree = OwnedMutTree::allocate( 3 );
  tree.at( 0 ) = limits( rt, gibi, mebi, 1 );
  tree.at( 1 ) = Handle<Strict>( handle::extract<Identification>( make_identification( compiler ) ).value() );
  tree.at( 2 ) = wasm;
  return rt.create( std::make_shared<OwnedTree>( std::move( tree ) ) ).visit<Handle<Application>>( []( auto x ) {
    return Handle<Application>( Handle<ExpressionTree>( x ) );
  } );
}

shared_ptr<Scheduler> scheduler = make_shared<LocalScheduler>();
auto rt = make_shared<Relater>( 1, nullopt, scheduler, false, 10000 );
Handle<Relation> add_fixpoint_evals[INIT_INSTANCE + 1];

#define ADD_TEST( func, message )                                                                                  \
  {                                                                                                                \
    cout << endl;                                                                                                  \
    cout << message << " for " << INIT_INSTANCE << " times" << endl;                                               \
    auto start = chrono::steady_clock::now();                                                                      \
    {                                                                                                              \
      for ( int i = 0; i < INIT_INSTANCE; i++ ) {                                                                  \
        func( i );                                                                                                 \
      }                                                                                                            \
    }                                                                                                              \
    auto end = chrono::steady_clock::now();                                                                        \
    cout << end - start << endl;                                                                                   \
    reset_global_timer();                                                                                          \
  }

#define ADD_FIX_TEST( func, message )                                                                              \
  {                                                                                                                \
    cout << endl;                                                                                                  \
    cout << message << " for " << INIT_INSTANCE << " times" << endl;                                               \
    reset_global_timer();                                                                                          \
    auto start = chrono::steady_clock::now();                                                                      \
    {                                                                                                              \
      for ( int i = 0; i < INIT_INSTANCE; i++ ) {                                                                  \
        func( i );                                                                                                 \
      }                                                                                                            \
    }                                                                                                              \
    auto end = chrono::steady_clock::now();                                                                        \
    cout << end - start << endl;                                                                                   \
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
void add_fixpoint( int );

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
  if ( argc < 2 ) {
    cerr << "Usage: " << argv[0] << " path_to_add_program\n";
    return -1;
  }

  add_program_name = argv[1];

  baseline_function();
  // Populate addends
  for ( int i = 0; i < INIT_INSTANCE + 1; i++ ) {
    arguments[i] = make_addend( i );
  }

  // Populate derived objects
  for ( int i = 0; i < INIT_INSTANCE; i++ ) {
    base_objects[i] = make_derived();
  }

  auto add_simple_8 = rt->direct_execute(
    Handle<Eval>( compile_application( *rt, file( *rt, "build/testing/wasm-examples/add-simple-8.wasm" ) ) ) );

  // Populate encodes
  for ( int i = 0; i < INIT_INSTANCE + 1; i++ ) {
    auto arg1 = blob( *rt, string_view( arguments[i].a, 1 ) );
    auto arg2 = blob( *rt, string_view( arguments[i].b, 1 ) );

    auto fix_combination = tree( *rt, limits( *rt, 1024 * 1024, 1024, 1 ).into<Fix>(), add_simple_8, arg1, arg2 )
                             .visit<Handle<ExpressionTree>>( []( auto h ) { return Handle<ExpressionTree>( h ); } );
    auto fix_eval = Handle<Eval>( Handle<Application>( fix_combination ) );

    add_fixpoint_evals[i] = fix_eval;
  }

  // Compile and link add-fixpoint and fix-wasi
  cout << "Executing trial run of add-simple-8" << endl;
  rt->direct_execute( add_fixpoint_evals[INIT_INSTANCE] );
  cout << "Done" << endl;

  reset_global_timer();
  ADD_TEST( static_add, "Executing add statically linked" );
  ADD_TEST( virtual_add, "Executing add via virtual function call" );
  ADD_FIX_TEST( add_fixpoint, "Executing add implemented in Fixpoint" );
  ADD_TEST( vfork_add, "Executing add program" );

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

void add_fixpoint( int i )
{
  rt->direct_execute( add_fixpoint_evals[i] );
}
