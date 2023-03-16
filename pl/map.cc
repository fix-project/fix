#include "util/fixpoint_util.cc"
#include <cstdint>
#include <string>

// Map applies main_blob to all the elements in arr. Passes the element as the fourth
// value in the encode and arg_1 as the fifth.
externref map( externref resource_limits, externref main_blob, uint32_t arr[], uint32_t arr_size, uint32_t arg_1 )
{
  int32_t old_size = grow_rw_table_1( arr_size, resource_limits );
  if ( old_size == -1 ) {
    return resource_limits;
  }

  externref arg_blob = create_blob_i32( arg_1 );
  for ( int i = 0; i < arr_size; i++ ) {
    old_size = grow_rw_table_0( 4, resource_limits );
    if ( old_size == -1 ) {
      return resource_limits;
    }
    set_rw_table_0( 0, resource_limits );
    set_rw_table_0( 1, main_blob );
    set_rw_table_0( 2, arg_blob );
    set_rw_table_0( 3, create_blob_i32( arr[i] ) );
    set_rw_table_1( i, create_thunk( create_tree_rw_table_0( 4 ) ) );
  }
  return create_tree_rw_table_1( arr_size );
}