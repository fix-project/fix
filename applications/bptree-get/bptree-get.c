#include "fixpoint_util.h"
#include "support.h"

#include <stdlib.h>

size_t upper_bound( uint64_t* keys, uint64_t size, uint64_t key )
{
  for ( size_t i = 0; i < size; i++ ) {
    if ( keys[i] > key ) {
      return i;
    }
  }

  return size;
}

__attribute__( ( export_name( "_fixpoint_apply" ) ) ) externref _fixpoint_apply( externref combination )
{
  attach_tree_ro_table_0( combination );

  externref key_h = get_ro_table_0( 4 );
  attach_blob_ro_mem_0( key_h );
  uint64_t key;
  ro_mem_0_to_program_mem( &key, 0, sizeof( uint64_t ) );

  externref keys_h = get_ro_table_0( 2 );
  attach_blob_ro_mem_0( keys_h );
  size_t size = byte_size_ro_mem_0();
  size_t num_of_keys = size / sizeof( uint64_t );
  uint64_t* keys = (uint64_t*)malloc( size );
  ro_mem_0_to_program_mem( keys, 0, size );

  externref childrenordata_h = get_ro_table_0( 3 );
  bool isleaf = ( num_of_keys + 1 == get_length( childrenordata_h ) );

  size_t idx = upper_bound( keys, num_of_keys, key );
  if ( isleaf ) {
    if ( idx != 0 && keys[idx - 1] == key ) {
      return create_selection_thunk( childrenordata_h, idx );
    } else {
      return create_blob_rw_mem_0( 0 );
    }
  } else {
    externref nextlevel = create_selection_thunk( childrenordata_h, idx + 1 );
    externref keyentry = create_selection_thunk( nextlevel, 0 );
    set_rw_table_0( 0, get_ro_table_0( 0 ) );
    set_rw_table_0( 1, get_ro_table_0( 1 ) );
    set_rw_table_0( 2, create_strict_encode( keyentry ) );
    set_rw_table_0( 3, create_shallow_encode( nextlevel ) );
    set_rw_table_0( 4, key_h );
    return create_application_thunk( create_tree_rw_table_0( 5 ) );
  }
}
