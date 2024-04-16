extern "C" {
#include "fixpoint_util.h"
#include "support.h"
}
#include <cstdlib>
#include <cstring>

/**
 * @brief Counts the words in the input blob.  Outputs a CSV file.
 * @return externref  A Thunk that evaluates to the result of the MapReduce operation.
 */
__attribute__( ( export_name( "_fixpoint_apply" ) ) ) externref _fixpoint_apply( externref combination )
{
  /* auto nil = create_blob_rw_mem_0( 0 ); */

  /* attach_tree_ro_table_0( combination ); */

  /* auto rlimits = get_ro_table_0( 0 ); */
  /* auto self = get_ro_table_0(1); */
  /* auto input = get_ro_table_0( 2 ); */

  /* attach_blob_ro_mem_0( input ); */
  /* auto size = get_length( input ); */
  /* char* p = (char*)malloc( 1 ); */
  /* ro_mem_0_to_program_mem( 0, p, size ); */
  /* auto x = strlen( p ); */
  return combination;

  /* return create_blob_i32( (int32_t)p ); */
}
