// This module contains fixpoint helper functions that do not need
// to be automatically generated. It uses the automatically generated functions from fixpoint_storage.cc
#include <stdbool.h>

#include "fixpoint_storage.h"

const static int WASM_RT_PAGESIZE = 65536;

extern externref create_blob_i32( int32_t number )
  __attribute__( ( import_module( "fixpoint" ), import_name( "create_blob_i32" ) ) );
extern externref create_blob_i64( int64_t number )
  __attribute__( ( import_module( "fixpoint" ), import_name( "create_blob_i64" ) ) );
extern externref create_application_thunk( externref pointer )
  __attribute__( ( import_module( "fixpoint" ), import_name( "create_application_thunk" ) ) );
extern externref create_selection_thunk( externref pointer, uint32_t idx )
  __attribute__( ( import_module( "fixpoint" ), import_name( "create_selection_thunk" ) ) );
extern externref create_selection_thunk_range( externref pointer, uint32_t begin_idx, uint32_t end_idx )
  __attribute__( ( import_module( "fixpoint" ), import_name( "create_selection_thunk_range" ) ) );
extern externref create_strict_encode( externref pointer )
  __attribute__( ( import_module( "fixpoint" ), import_name( "create_strict_encode" ) ) );
extern externref create_shallow_encode( externref pointer )
  __attribute__( ( import_module( "fixpoint" ), import_name( "create_shallow_encode" ) ) );
extern externref create_tag( externref object, externref blob )
  __attribute__( ( import_module( "fixpoint" ), import_name( "create_tag" ) ) );

extern void fixpoint_unsafe_io( const char* s, int32_t size )
  __attribute__( ( import_module( "fixpoint" ), import_name( "memory_unsafe_io" ) ) );

extern bool fixpoint_is_equal( externref x, externref y )
  __attribute__( ( import_module( "fixpoint" ), import_name( "is_equal" ) ) );
extern bool fixpoint_is_blob( externref x )
  __attribute__( ( import_module( "fixpoint" ), import_name( "is_blob" ) ) );

extern uint32_t get_length( externref handle )
  __attribute__( ( import_module( "fixpoint" ), import_name( "get_length" ) ) );

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

int32_t get_i32_ro_mem( int32_t mem_id, int32_t index );

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

int32_t size_rw_table( int32_t table_id );
