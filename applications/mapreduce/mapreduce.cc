extern "C" {
#include "fixpoint_util.h"
}
#include <cassert>

extern externref create_identification_thunk( externref pointer )
  __attribute__( ( import_module( "fixpoint" ), import_name( "create_identification_thunk" ) ) );

externref mapreduce( externref mapper,
                     externref reducer,
                     externref rlimitsm,
                     externref rlimitsr,
                     int start,
                     int end )
{
  auto nil = create_blob_rw_mem_0( 0 );
  if ( start == end or start == end - 1 ) {
    grow_rw_table_0( 3, nil );
    set_rw_table_0( 0, rlimitsm );
    set_rw_table_0( 1, mapper );
    set_rw_table_0( 2, create_strict_encode( create_identification_thunk( get_ro_table_0( start ) ) ) );
    return create_application_thunk( create_tree_rw_table_0( 3 ) );
  } else {
    auto split = start + ( end - start ) / 2;
    externref first = create_strict_encode( mapreduce( mapper, reducer, rlimitsm, rlimitsr, start, split ) );
    externref second = create_strict_encode( mapreduce( mapper, reducer, rlimitsm, rlimitsr, split, end ) );
    grow_rw_table_0( 4, nil );
    set_rw_table_0( 0, rlimitsr );
    set_rw_table_0( 1, reducer );
    set_rw_table_0( 2, first );
    set_rw_table_0( 3, second );
    return create_application_thunk( create_tree_rw_table_0( 4 ) );
  }
}

struct rlimits
{
  uint64_t memory;
  uint64_t output_size;
  uint64_t output_fanout;
};
static_assert( sizeof( rlimits ) == 24 );

/**
 * @brief Applies a mapper and a reducer to a Tree, producing a single combined output.
 * @details The mapper is the second argument.
 *          The reducer is the third argument.
 *          The target is the fourth argument.
 *          The rlimits to use for the mapper are the fifth argument.
 *          The rlimits to use for the reducer are the sixth argument.
 * @return externref  A Thunk that evaluates to the result of the MapReduce operation.
 */
__attribute__( ( export_name( "_fixpoint_apply" ) ) ) externref _fixpoint_apply( externref encode )
{
  auto nil = create_blob_rw_mem_0( 0 );

  attach_tree_ro_table_0( encode );

  /* auto rlimits = get_ro_table_0( 0 ); */
  /* auto self = get_ro_table_0(1); */
  auto mapper = get_ro_table_0( 2 );
  auto reducer = get_ro_table_0( 3 );
  auto target = get_ro_table_0( 4 );
  auto rlimitsm = get_ro_table_0( 5 );
  auto rlimitsr = get_ro_table_0( 5 );

  attach_tree_ro_table_0( target );
  auto N = get_length( target );
  return mapreduce( mapper, reducer, rlimitsm, rlimitsr, 0, N );
}
