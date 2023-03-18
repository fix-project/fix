#include "fixpoint_util.h"

void copy_ro_to_rw_mem( int32_t rw_mem_id, int32_t ro_mem_id, int32_t rw_offset, int32_t ro_offset, int32_t len )
{
  for ( int i = 0; i < len; i++ ) {
    int32_t val = get_i8_ro_mem_functions[ro_mem_id]( ro_offset + i );
    set_i8_rw_mem_functions[rw_mem_id]( rw_offset + i, val );
  }
}

void copy_ro_to_rw_table( int32_t rw_table_id,
                          int32_t ro_table_id,
                          int32_t rw_offset,
                          int32_t ro_offset,
                          int32_t len )
{
  for ( int i = 0; i < len; i++ ) {
    externref val = get_ro_table_functions[ro_table_id]( ro_offset + i );
    set_rw_table_functions[rw_table_id]( rw_offset + i, val );
  }
}

externref get_ro_table( int32_t table_id, int32_t index )
{
  return get_ro_table_functions[table_id]( index );
}
void attach_tree_ro_table( int32_t table_id, externref name )
{
  attach_tree_functions[table_id]( name );
}

void set_rw_table( int32_t table_id, int32_t index, externref val )
{
  set_rw_table_functions[table_id]( index, val );
}

void set_i32_rw_mem( int32_t mem_id, int32_t index, int32_t val )
{
  set_i32_rw_mem_functions[mem_id]( index, val );
}

int32_t get_i32_rw_mem( int32_t mem_id, int32_t index )
{
  return get_i32_rw_mem_functions[mem_id]( index );
}

int32_t size_ro_mem( int32_t mem_id )
{
  return size_ro_mem_functions[mem_id]();
}

int32_t size_ro_table( int32_t table_id )
{
  return size_ro_table_functions[table_id]();
}

void attach_blob_ro_mem( int32_t mem_id, externref blob )
{
  attach_blob_functions[mem_id]( blob );
}

externref create_blob_rw_mem( int32_t mem_id, int32_t length )
{
  return create_blob_functions[mem_id]( length );
}

externref create_tree_rw_table( int32_t table_id, int32_t length )
{
  return create_tree_functions[table_id]( length );
}

int32_t grow_rw_mem( int32_t mem_id, int32_t delta )
{
  return grow_rw_mem_functions[mem_id]( delta );
}

int32_t grow_rw_table( int32_t table_id, int32_t delta, externref init_value )
{
  return grow_rw_table_functions[table_id]( delta, init_value );
}
