#include <stdio.h>

#include "test.hh"

using namespace std;

#define CHECK( condition )                                                                                         \
  if ( not( condition ) ) {                                                                                        \
    fprintf( stderr, "%s:%d - assertion '%s' failed\n", __FILE__, __LINE__, #condition );                          \
    exit( 1 );                                                                                                     \
  }

int main( int, char** )
{
  auto& rt = Runtime::get_instance();
  auto& s = rt.storage();

  Handle data = tree( {
    blob( "visible 1" ),
    blob( "visible 2" ),
    tree( { blob( "not visible" ) } ).as_lazy(),
    blob( "visible 3" ),
    thunk( tree( {
             blob( "unused" ),
             blob( "elf" ),
           } ) )
      .as_lazy(),
  } );

  size_t count = 0;
  s.visit(
    data,
    [&]( Handle h ) {
      count++;
      CHECK( not s.compare_handles( h, blob( "not visible" ) ) );
      CHECK( not s.compare_handles( h, blob( "unused" ) ) );
      CHECK( not s.compare_handles( h, blob( "elf" ) ) );
    },
    true,
    true );
  CHECK( count == 6 );

  count = 0;
  s.visit(
    data,
    [&]( Handle h ) {
      count++;
      CHECK( not s.compare_handles( h, blob( "not visible" ) ) );
      CHECK( not s.compare_handles( h, blob( "unused" ) ) );
      CHECK( not s.compare_handles( h, blob( "elf" ) ) );
      CHECK( not s.compare_handles( h, tree( { blob( "not visible" ) } ).as_lazy() ) );
      CHECK( not h.is_lazy() );
      CHECK( not h.is_thunk() );
    },
    false,
    true );
  CHECK( count == 4 );

  count = 0;
  s.visit(
    data,
    [&]( Handle h ) {
      count++;
      CHECK( not s.compare_handles( h, blob( "not visible" ) ) );
      CHECK( not s.compare_handles( h, blob( "unused" ) ) );
      CHECK( not s.compare_handles( h, blob( "elf" ) ) );
      CHECK( not h.is_thunk() );
    },
    true,
    false );
  CHECK( count == 5 );
}
