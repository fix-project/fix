extern "C" {
#include "fixpoint_util.h"
}
#include <cassert>

externref mapreduce( externref mapper, externref reducer, externref rlimits, int start, int end )
{
  auto nil = create_blob_rw_mem_0( 0 );
  if ( start == end or start == end - 1 ) {
    grow_rw_table_0( 3, nil );
    set_rw_table_0( 0, rlimits );
    set_rw_table_0( 1, mapper );
    set_rw_table_0( 2, get_ro_table_0( start ) );
    return create_application_thunk( create_tree_rw_table_0( 3 ) );
  } else {
    auto split = start + ( end - start ) / 2;
    externref first = create_strict_encode( mapreduce( mapper, reducer, rlimits, start, split ) );
    externref second = create_strict_encode( mapreduce( mapper, reducer, rlimits, split, end ) );
    grow_rw_table_0( 4, nil );
    set_rw_table_0( 0, rlimits );
    set_rw_table_0( 1, reducer );
    set_rw_table_0( 2, first );
    set_rw_table_0( 3, second );
    return create_application_thunk( create_tree_rw_table_0( 4 ) );
  }
}

/**
 * @brief Applies a mapper and a reducer to a Tree, producing a single combined output.
 * @details The mapper is the second argument.
 *          The reducer is the third argument.
 *          The target is the fourth argument.
 * @return externref  A Thunk that evaluates to the result of the MapReduce operation.
 */
__attribute__( ( export_name( "_fixpoint_apply" ) ) ) externref _fixpoint_apply( externref encode )
{
  auto nil = create_blob_rw_mem_0( 0 );

  attach_tree_ro_table_0( encode );

  auto rlimits = get_ro_table_0( 0 );
  /* auto self = get_ro_table_0(1); */
  auto mapper = get_ro_table_0( 2 );
  auto reducer = get_ro_table_0( 3 );
  auto target = get_ro_table_0( 4 );

  attach_tree_ro_table_0( target );
  auto N = get_length( target );
  return mapreduce( mapper, reducer, rlimits, 0, N );
}
