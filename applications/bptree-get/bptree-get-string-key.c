#pragma clang diagnostic ignored "-Wignored-attributes"

#include "fixpoint_util.h"
#include "support.h"

#include <stdlib.h>
#include <string.h>

int upper_bound( char** keys, int size, char* key )
{
  for ( int i = 0; i < size; i++ ) {
    if ( strcmp( keys[i], key ) > 0 ) {
      return i;
    }
  }

  return size;
}

char* blob_to_str( externref blob )
{
  attach_blob_ro_mem_0( blob );
  int size = byte_size_ro_mem_0();
  char* buf = (char*)malloc( size + 1 );
  ro_mem_0_to_program_mem( buf, 0, size );
  buf[size] = '\0';
  return buf;
}

__attribute__( ( export_name( "_fixpoint_apply" ) ) ) externref _fixpoint_apply( externref combination )
{
  attach_tree_ro_table_0( combination );

  externref key_h = get_ro_table_0( 4 );
  char* key = blob_to_str( key_h );

  externref keys_h = get_ro_table_0( 2 );
  attach_blob_ro_mem_0( keys_h );
  bool isleaf;
  ro_mem_0_to_program_mem( &isleaf, 0, 1 );

  int keys_byte_size = byte_size_ro_mem_0();
  char* keys_buf = (char*)malloc( keys_byte_size );
  ro_mem_0_to_program_mem( keys_buf, 1, keys_byte_size - 1 );

  externref childrenordata_h = get_ro_table_0( 3 );
  int num_of_keys = get_length( childrenordata_h );
  char** keys = (char**)malloc( num_of_keys * sizeof( char* ) );

  int real_num_of_keys = 0;
  int offset = 0;
  while ( offset < keys_byte_size - 1 ) {
    uint64_t entry_size;
    memcpy( &entry_size, keys_buf + offset, sizeof( uint64_t ) );
    offset += sizeof( uint64_t );

    keys[real_num_of_keys] = (char*)malloc( entry_size + 1 );
    memcpy( keys[real_num_of_keys], keys_buf + offset, entry_size );
    offset += entry_size;
    keys[real_num_of_keys][entry_size] = '\0';

    real_num_of_keys += 1;
  }

  int idx = upper_bound( keys, real_num_of_keys, key );

  if ( isleaf ) {
    if ( idx != 0 && strcmp( keys[idx - 1], key ) == 0 ) {
      return create_selection_thunk( childrenordata_h, idx );
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
    return create_application_thunk( create_tree_rw_table_0( 5 ) );
  }
}
