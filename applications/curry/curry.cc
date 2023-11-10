extern "C" {
#include "../util/fixpoint_util.h"
}
#include <cstdint>
#include <string>

enum VALUE_TYPE
{
  Tree,
  Thunk,
  Blob,
  Tag
};

externref run( externref resource_limits, externref joined_encode )
{

  attach_tree_ro_table_0( joined_encode );
  externref curried_resource_limits = get_ro_table_0( 0 );
  externref program = get_ro_table_0( 2 );

  attach_blob_ro_mem_0( get_ro_table_1( 3 ) );
  int32_t num_args = get_i32_ro_mem_0( 0 );

  int32_t old_size = grow_rw_table_0( num_args + 2, resource_limits ); // +2 for resource_limits and program
  if ( old_size == -1 ) {
    return resource_limits;
  }

  set_rw_table_0( 0, curried_resource_limits );
  set_rw_table_0( 1, program );
  for ( int32_t i = 0; i < num_args; i++ ) {
    set_rw_table_0( i + 2, get_ro_table_0( i + 4 ) );
  }

  return create_thunk( create_tree_rw_table_0( num_args + 2 ) );
}

externref apply( externref encode )
{

  attach_tree_ro_table_0( encode );

  externref resource_limits = get_ro_table_0( 0 );
  externref sub_encode = get_ro_table_0( 1 );

  int32_t tree_length = get_length( sub_encode ) + get_length( encode ) - 2;

  attach_tree_ro_table_1( sub_encode );
  attach_blob_ro_mem_0( get_ro_table_1( 3 ) );
  int32_t num_args = get_i32_ro_mem_0( 0 );

  int32_t old_size = grow_rw_table_0( tree_length, resource_limits );
  if ( old_size == -1 ) {
    return resource_limits;
  }

  for ( uint32_t i = 0; i < get_length( sub_encode ); i++ ) { // Copy sub_encode
    set_rw_table_0( i, get_ro_table_1( i ) );
  }

  for ( uint32_t i = 2; i < get_length( encode ); i++ ) { // Copy encode params
    set_rw_table_0( i + get_length( sub_encode ) - 2, get_ro_table_0( i ) );
  }

  externref joined_encode = create_tree_rw_table_0( tree_length );

  if ( tree_length - 4 >= num_args ) {
    return run( resource_limits, joined_encode );
  }

  return joined_encode;
}

__attribute__( ( export_name( "_fixpoint_apply" ) ) ) externref _fixpoint_apply( externref encode )
{
  attach_tree_ro_table_0( encode );
  if ( get_value_type( get_ro_table_0( 1 ) ) == Tag ) { // Un-curried function
    return encode;
  }
  
  return apply( encode );
}
