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
 * @brief Tags a value with a boolean indicating whether the program succeeded.
 *
 * @param value       The value to tag.
 * @param succeeded   Whether the program succeeded.
 * @return externref  The tagged value.
 */
externref tagged_output( externref value, bool succeeded )
{
  grow_rw_mem_0_pages( 4 );
  return create_tag( value, create_blob_i32( succeeded ) );
}

/**
 * @brief Runs a curried function
 *
 * @param resource_limits   The current resource limits.
 * @param joined_encode     Tree containing (in order) initial resource_limits, current program, program to run,
 *                          number of required arguments, and arguments.
 * @return externref        The thunk to the full apply tagged as success, or the curried function tagged as a
 * failure.
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
    return tagged_output( joined_encode, false );
  }

  set_rw_table_0( 0, curried_resource_limits );
  set_rw_table_0( 1, program );
  for ( int32_t i = 0; i < num_args; i++ ) {
    set_rw_table_0( i + 2, get_ro_table_0( i + 4 ) );
  }

  return create_application_thunk( create_tree_rw_table_0( num_args + 2 ) );
}

/**
 * @brief Applies the provided arguments to a curried function
 *
 * @param encode      The program being run, with a sub_encode as the second argument.
 * @return externref  The curried function as a tree if a partial apply tagged as successful if the arguments are
 * applied, or a thunk if a full apply tagged successful.
 */
externref apply( externref encode )
{
  attach_tree_ro_table_0( encode );

  externref resource_limits = get_ro_table_0( 0 );

  attach_tree_ro_table_1( get_ro_table_0( 1 ) );
  externref sub_encode = get_ro_table_1( 1 );

  int32_t tree_length = get_length( sub_encode ) + get_length( encode ) - 2;

  attach_tree_ro_table_1( sub_encode );
  attach_blob_ro_mem_0( get_ro_table_1( 3 ) );
  int32_t num_args = get_i32_ro_mem_0( 0 );

  int32_t old_size = grow_rw_table_0( tree_length, resource_limits );
  if ( old_size == -1 ) {
    return tagged_output( sub_encode, false );
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

  return tagged_output( joined_encode, true );
}

/**
 * @brief Converts a function into a curried function, or applies the arguments to a curried function
 * @details Passes the function to curry or the tagged curried function as the 1st argument.
 *          If creating a curried function, passes the number of arguments as the 2nd argument.
 *          If applying arguments, passes the arguments as all subsequent arguments.
 * @return externref  A callable curried function tagged based on success, or a thunk to the
 *                    full apply tagged successful if enough arguments are provided.
 */
__attribute__( ( export_name( "_fixpoint_apply" ) ) ) externref _fixpoint_apply( externref encode )
{
  attach_tree_ro_table_0( encode );
  attach_tree_ro_table_1( get_ro_table_0( 1 ) ); // Attach current program tag

  if ( fixpoint_is_blob( get_ro_table_1( 1 ) ) ) { // Initial (unwrapped) call
    return tagged_output( encode, true );
  }

  return apply( encode );
}
