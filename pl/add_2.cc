#include "map.cc"

int32_t add_2( int32_t x )
{
  return x + 2;
}

/* _fixpoint_apply takes an encode with the follow structure:
- resource limits
- this program
- add_flag: if 1, the program executes add_2, else execute map
- [optional] x: if add_flag, returns x + 2
*/
__attribute__( ( export_name( "_fixpoint_apply" ) ) ) externref _fixpoint_apply( externref encode )
{
  attach_tree_ro_table_0( encode );
  attach_blob_ro_mem_0( get_ro_table_0( 2 ) );
  uint32_t add_flag = get_ro_mem_0( 0 );

  if ( add_flag == 1 ) {
    attach_blob_ro_mem_0( get_ro_table_0( 3 ) );
    int32_t blob_value = get_ro_mem_0( 0 );
    return create_blob_i32( add_2( blob_value ) );
  }

  uint32_t arr[3] = { 4, 8, 1 };
  uint32_t arr_size = sizeof( arr ) / sizeof( arr[0] );
  externref resource_limits = get_ro_table_0( 0 );
  externref main_blob = get_ro_table_0( 1 );
  return map( resource_limits, main_blob, arr, arr_size, 1 );
}