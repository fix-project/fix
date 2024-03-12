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

  auto eval = []( auto x ) { return Handle<Eval>( x ); };
  auto apply = []( auto x ) { return Handle<Apply>( x ); };

  absl::flat_hash_set<DependencyGraph::Task> ready;
  CHECK( graph.start( apply( otree ) ) );
  CHECK( graph.contains( apply( otree ) ) );
  graph.finish( apply( otree ), ready );
  CHECK( not graph.contains( apply( otree ) ) );
  CHECK( ready.empty() );

  CHECK( graph.start( apply( otree ) ) );
  CHECK( not graph.start( apply( otree ) ) );
  graph.add_dependency( apply( otree ), eval( foo ) );
  CHECK( graph.start( eval( foo ) ) );
  graph.add_dependency( apply( otree ), eval( bar ) );
  CHECK( graph.start( eval( bar ) ) );
  graph.add_dependency( apply( otree ), eval( bar ) );
  CHECK( not graph.start( eval( bar ) ) );
  graph.finish( eval( bar ), ready );
  CHECK( ready.empty() );
  graph.finish( eval( baz ), ready );
  CHECK( ready.empty() );
  graph.finish( eval( foo ), ready );
  CHECK( not ready.empty() );
  CHECK( ready.contains( apply( otree ) ) );
  CHECK( ready.size() == 1 );
}
