extern "C" {
#include "../util/fixpoint_util.h"
}
#include <cstdint>
#include <string>

typedef enum VALUE_TYPE
{
  Tree,
  Thunk,
  Blob,
  Tag
} VALUE_TYPE;

/**
 * @brief Runs a curried function
 *
 * @param resource_limits - current resource_limits
 * @param joined_encode - tree containing (in order) initial resource_limits, current program, program to run,
 * number of required arguments, and arguments
 * @return externref - a thunk to the full apply, or resource_limits if the program was aborted
 */
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

/**
 * @brief Applies the provided arguments to a curried function
 *
 * @param encode - the program being run, with a sub_encode as the second argument
 * @return externref - the curried function as a tree if a partial apply, a thunk if a full apply,
 * or resource_limits if the program was aborted
 */
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

/**
 * @brief Converts a function into a curried function, or applies the arguments to a curried function
 * @details Passes the function to curry or the curried function as the 1st argument.
 *  If creating a curried function, passes the number of arguments as the 2nd argument. If applying arguments,
 * passes the arguments as all subsequent arguments.
 * @return externref - a callable curried function, resource_limits if the program was aborted, or a thunk to the full apply if
 * enough arguments are provided
 */
__attribute__( ( export_name( "_fixpoint_apply" ) ) ) externref _fixpoint_apply( externref encode )
{
  attach_tree_ro_table_0( encode );

  if ( get_value_type( get_ro_table_0( 1 ) ) == VALUE_TYPE::Tag ) { // Initial (unwrapped) call
    return encode;
  }

  return apply( encode );
}
