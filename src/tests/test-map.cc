#include <stdio.h>

#include "relater.hh"
#include "test.hh"

using namespace std;

void test( shared_ptr<Relater> rt )
{
  auto t = Handle<Application>( handle::upcast(
    tree( *rt,
          limits( *rt, 1024 * 1024, 1024, 1 ),
          compile( *rt, file( *rt, "applications-prefix/src/applications-build/map/add_2_map.wasm" ) ),
          Handle<Literal>( 0 ) ) ) );

  auto result = rt->execute( Handle<Eval>( t ) );
  auto tree = rt->get( result.try_into<ValueTree>().value() );
  uint32_t results[3];
  for ( size_t i = 0; i < 3; i++ ) {
    memcpy( &results[i], handle::extract<Literal>( tree.value()->at( i ) )->data(), sizeof( uint32_t ) );
  }
  if ( results[0] == 6 and results[1] == 10 and results[2] == 3 ) {
    exit( 0 );
  }
  printf( "Results did not match: {%d, %d, %d} != {6, 10, 3}\n", results[0], results[1], results[2] );
}
