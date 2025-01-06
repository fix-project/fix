#pragma clang diagnostic ignored "-Wignored-attributes"

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

  size_t argc = get_length( combination );
  if ( argc != 4 ) {
    out( "incorrect number of arguments to merge_counts, expected: count_x, count_y\n" );
    return create_blob_i64( -1 );
  }

  /* auto rlimits = get_ro_table_0( 0 ); */
  /* auto self = get_ro_table_0( 1 ); */
  externref X = get_ro_table_0( 2 );
  externref Y = get_ro_table_0( 3 );

  attach_blob_ro_mem_0( X );
  attach_blob_ro_mem_1( Y );
  uint64_t x;
  uint64_t y;
  if ( get_length( X ) != sizeof( uint64_t ) || get_length( Y ) != sizeof( uint64_t ) ) {
    out( "incorrect type of arguments to merge_counts, expected: u64, u64\n" );
    return create_blob_i64( -1 );
  }
  ro_mem_0_to_program_mem( &x, 0, sizeof( uint64_t ) );
  ro_mem_1_to_program_mem( &y, 0, sizeof( uint64_t ) );
  return create_blob_i64( x + y );
}
