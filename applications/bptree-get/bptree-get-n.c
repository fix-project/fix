#include "fixpoint_util.h"
#include "support.h"

#include <stdlib.h>

int upper_bound( int* keys, uint64_t size, int key )
{
  for ( int i = 0; i < size; i++ ) {
    if ( keys[i] > key ) {
      return i;
    }
  }

  return size;
}

__attribute__( ( export_name( "_fixpoint_apply" ) ) ) externref _fixpoint_apply( externref combination )
{
  externref nil = create_tree_rw_table_1( 0 );

  attach_tree_ro_table_0( combination );

  externref key_h = get_ro_table_0( 4 );
  attach_blob_ro_mem_0( key_h );
  int key;
  ro_mem_0_to_program_mem( &key, 0, sizeof( key ) );

  externref keys_h = get_ro_table_0( 2 );
  attach_blob_ro_mem_0( keys_h );
  int size = byte_size_ro_mem_0();
  int num_of_keys = size / sizeof( key );

  bool isleaf;
  int* keys = (int*)malloc( size );
  ro_mem_0_to_program_mem( &isleaf, 0, 1 );
  ro_mem_0_to_program_mem( keys, 1, size - 1 );

  externref childrenordata_h = get_ro_table_0( 3 );

  int idx = upper_bound( keys, num_of_keys, key );
  if ( isleaf ) {
    if ( idx != 0 && keys[idx - 1] == key ) {
      externref n_h = get_ro_table_0( 5 );
      attach_blob_ro_mem_0( n_h );
      uint64_t n;
      ro_mem_0_to_program_mem( &n, 0, sizeof( uint64_t ) );

      if ( n > 6 ) {
        grow_rw_table_0( n - 6, nil );
      }

      set_rw_table_0( 0, create_selection_thunk_range( childrenordata_h, idx, size_ro_table_0() - 1 ) );
      for ( uint64_t i = 1; i < n; i++ ) {
        childrenordata_h = create_selection_thunk( childrenordata_h, size_ro_table_0() - 1 );
        set_rw_table_0( i, create_selection_thunk_range( childrenordata_h, 1, size_ro_table_0() - 1 ) );
      }
      return create_tree_rw_table_0( n );

    } else {
      return create_tree_rw_table_0( 0 );
    }
  } else {
    externref nextlevel = create_selection_thunk( childrenordata_h, idx + 1 );
    externref keyentry = create_selection_thunk( nextlevel, 0 );
    set_rw_table_0( 0, get_ro_table_0( 0 ) );
    set_rw_table_0( 1, get_ro_table_0( 1 ) );
    set_rw_table_0( 2, create_strict_encode( keyentry ) );
    set_rw_table_0( 3, create_shallow_encode( nextlevel ) );
    set_rw_table_0( 4, key_h );
    set_rw_table_0( 5, get_ro_table_0( 5 ) );
    return create_application_thunk( create_tree_rw_table_0( 6 ) );
  }
}
