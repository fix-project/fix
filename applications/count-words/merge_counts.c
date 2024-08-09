#include "fixpoint_util.h"
#include "support.h"

#include <stdlib.h>
#include <string.h>

void out( const char* s )
{
  fixpoint_unsafe_io( s, strlen( s ) );
}

__attribute__( ( export_name( "_fixpoint_apply" ) ) ) externref _fixpoint_apply( externref combination )
{
  attach_tree_ro_table_0( combination );

  /* auto rlimits = get_ro_table_0( 0 ); */
  /* auto self = get_ro_table_0( 1 ); */
  externref X = get_ro_table_0( 2 );
  externref Y = get_ro_table_0( 3 );

  attach_blob_ro_mem_0( X );
  attach_blob_ro_mem_1( Y );
  size_t sX = get_length( X );
  size_t sY = get_length( Y );

  uint64_t fX[256];
  uint64_t fY[256];
  ro_mem_0_to_program_mem( fX, 0, sX );
  ro_mem_1_to_program_mem( fY, 0, sY );

  uint64_t f[256];
  for ( int i = 0; i < 256; i++ ) {
    f[i] = fX[i] + fY[i];
  }

  if ( grow_rw_mem_0_pages( 1 ) == -1 ) {
    out( "merge counts: grow error" );
  }
  program_mem_to_rw_mem_0( 0, f, sizeof( f ) );

  return create_blob_rw_mem_0( sizeof( f ) );
}
