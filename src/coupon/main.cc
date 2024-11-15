#include "handle.hh"
#include "overload.hh"
#include "runtime.hh"
#include "scheduler.hh"
#include <iostream>
#include <variant>

using namespace std;

TestRuntime test;

template<FixHandle... Args>
Handle<Application> application( Handle<Fix> ( *f )( Handle<Tree> ), Args... args )
{
  OwnedMutTree tree = OwnedMutTree::allocate( sizeof...( args ) + 2 );
  tree[0] = Handle<Literal>( (uint64_t)0 );
  tree[1] = Handle<Literal>( (uint64_t)f );
  size_t i = 2;
  (
    [&] {
      tree[i] = args;
      i++;
    }(),
    ... );
  return Handle<Application>( test.create_tree( std::make_shared<OwnedTree>( std::move( tree ) ) ) );
}

Handle<Fix> add( Handle<Tree> combination )
{
  DeterministicEquivRuntime& rt = (DeterministicEquivRuntime&)( (DeterministicTagRuntime&)test );

  auto data = rt.attach( combination );
  uint64_t x( data->at( 2 ).unwrap<Value>().unwrap<Blob>().unwrap<Literal>() );
  uint64_t y( data->at( 3 ).unwrap<Value>().unwrap<Blob>().unwrap<Literal>() );
  cout << x << " + " << y << " = " << x + y << endl;
  return Handle<Literal>( x + y );
}

Handle<Fix> fib( Handle<Tree> combination )
{
  DeterministicEquivRuntime& rt = (DeterministicEquivRuntime&)( (DeterministicTagRuntime&)test );

  auto data = rt.attach( combination );
  uint64_t x( data->at( 2 ).unwrap<Value>().unwrap<Blob>().unwrap<Literal>() );
  cout << "fib(" << x << ")" << endl;
  if ( x < 2 ) {
    return Handle<Literal>( x );
  } else {
    auto a = Handle<Strict>( application( fib, Handle<Literal>( x - 1 ) ) );
    auto b = Handle<Strict>( application( fib, Handle<Literal>( x - 2 ) ) );
    return application( add, a, b );
  }
}

int main( int, char** )
{
  uint64_t a = 4;

  Tagger tagger( test );
  LocalScheduler scheduler( test, tagger );

  SchedulerContinuation cont;
  cont.top_level_job = application( fib, Handle<Literal>( a ) );
  deque<KernelExecutionTag> results;

  auto sum = scheduler.schedule( cont.top_level_job, cont.continuation, results );

  cout << sum.lhs << " eval to " << sum.rhs << endl;
}
