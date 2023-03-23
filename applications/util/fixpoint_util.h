// This module contains fixpoint helper functions that do not need
// to be automatically generated. It uses the automatically generated functions from fixpoint_storage.cc

#include "fixpoint_storage.h"

const static int WASM_RT_PAGESIZE = 65536;

externref create_blob_i32( int32_t number )
  __attribute__( ( import_module( "fixpoint" ), import_name( "create_blob_i32" ) ) );
externref create_thunk( externref pointer )
  __attribute__( ( import_module( "fixpoint" ), import_name( "create_thunk" ) ) );
void fixpoint_unsafe_io( const char* s, int32_t size )
  __attribute__( ( import_module( "fixpoint" ), import_name( "unsafe_io" ) ) );
extern int32_t value_type( externref ) __attribute( ( import_module( "fixpoint" ), import_name( "value_type" ) ) );

// copies memory from ro_mem_[ro_mem_id] to rw_mem_[ro_mem_id].
void copy_ro_to_rw_mem( int32_t rw_mem_id, int32_t ro_mem_id, int32_t rw_offset, int32_t ro_offset, int32_t len );

// copies table from ro_table_[ro_table_id] to rw_table_[ro_table_id].
void copy_ro_to_rw_table( int32_t rw_table_id,
                          int32_t ro_table_id,
                          int32_t rw_offset,
                          int32_t ro_offset,
                          int32_t len );

externref get_ro_table( int32_t table_id, int32_t index ) __attribute__( ( __export_name__( "get_ro_table" ) ) );

void attach_tree_ro_table( int32_t table_id, externref name );

void set_rw_table( int32_t table_id, int32_t index, externref val );

void set_i32_rw_mem( int32_t mem_id, int32_t index, int32_t val );

int32_t get_i32_rw_mem( int32_t mem_id, int32_t index );

// Returns the size of mem_id in PAGES (not bytes)
int32_t byte_size_ro_mem( int32_t mem_id );

int32_t size_ro_table( int32_t table_id );

void attach_blob_ro_mem( int32_t mem_id, externref blob );

// mem_id is size zero after this function is called.
externref create_blob_rw_mem( int32_t mem_id, int32_t length );

// table_id is size zero after this function is called.
externref create_tree_rw_table( int32_t table_id, int32_t length );

int32_t grow_rw_mem_pages( int32_t mem_id, int32_t delta );

int32_t grow_rw_table( int32_t table_id, int32_t delta, externref init_value );

int32_t page_size_rw_mem( int32_t mem_id );
