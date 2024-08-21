#include <glog/logging.h>

#include "dependency_graph.hh"
#include "handle.hh"

using namespace std;

void test( void )
{
  DependencyGraph graph;

  Handle foo = "foo"_literal;
  Handle bar = "bar"_literal;
  Handle baz = "baz"_literal;

  Handle otree = Handle<ObjectTree>::nil();
  Handle application = Handle<Thunk>( Handle<Application>( Handle<ExpressionTree>( otree ) ) );

  auto eval = []( auto x ) { return Handle<Eval>( x ); };
  auto step = []( auto x ) { return Handle<Think>( x ); };

  absl::flat_hash_set<DependencyGraph::Task> ready;
  CHECK( graph.start( step( application ) ) );
  CHECK( graph.contains( step( application ) ) );
  graph.finish( step( application ), ready );
  CHECK( not graph.contains( step( application ) ) );
  CHECK( ready.empty() );

  CHECK( graph.start( step( application ) ) );
  CHECK( not graph.start( step( application ) ) );
  graph.add_dependency( step( application ), eval( foo ) );
  CHECK( graph.start( eval( foo ) ) );
  graph.add_dependency( step( application ), eval( bar ) );
  CHECK( graph.start( eval( bar ) ) );
  graph.add_dependency( step( application ), eval( bar ) );
  CHECK( not graph.start( eval( bar ) ) );
  graph.finish( eval( bar ), ready );
  CHECK( ready.empty() );
  graph.finish( eval( baz ), ready );
  CHECK( ready.empty() );
  graph.finish( eval( foo ), ready );
  CHECK( not ready.empty() );
  CHECK( ready.contains( step( application ) ) );
  CHECK( ready.size() == 1 );
}
