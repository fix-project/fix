// This module contains fixpoint helper functions that do not need
// to be automatically generated. Imports automatically generated functions from fixpoint_storage.cc
#include <string>

extern "C" {
  #include "fixpoint_storage.c"
}

int WASM_RT_PAGESIZE = 65536;

externref create_blob_i32( uint32_t number )
  __attribute__( ( import_module( "fixpoint" ), import_name( "create_blob_i32" ) ) );
externref create_thunk( externref pointer )
  __attribute__( ( import_module( "fixpoint" ), import_name( "create_thunk" ) ) );
void fixpoint_unsafe_io( const char* s, int32_t size )
  __attribute__( ( import_module( "fixpoint" ), import_name( "unsafe_io" ) ) );

void unsafe_io( std::string s )
{
  fixpoint_unsafe_io( s.c_str(), s.length() );
}

// copy_ro_to_rw_mem copies memory from ro_mem_[ro_mem_id] to rw_mem_[ro_mem_id].
void copy_ro_to_rw_mem( int32_t ro_mem_id, int32_t rw_mem_id, int32_t ro_offset, int32_t rw_offset, int32_t len )
{
  if ( size_ro_mem_functions[ro_mem_id]() * WASM_RT_PAGESIZE < ro_offset + len ) {
    unsafe_io( "Cannot copy from ro mem to rw mem. Ro out of bounds." );
  }
  if ( size_rw_mem_functions[ro_mem_id]() * WASM_RT_PAGESIZE < rw_offset + len ) {
    unsafe_io( "Cannot copy from ro mem to rw mem. Rw out of bounds." );
  }
  for ( int i = 0; i < len; i++ ) {
    int32_t val = get_i8_ro_mem_functions[ro_mem_id]( ro_offset + i );
    set_i8_rw_mem_functions[rw_mem_id]( rw_offset + i, val );
  }
}

// copy_ro_to_rw_table copies table from ro_table_[ro_table_id] to rw_table_[ro_table_id].
void copy_ro_to_rw_table( int32_t ro_table_id,
                          int32_t rw_table_id,
                          int32_t ro_offset,
                          int32_t rw_offset,
                          int32_t len )
{
  if ( size_ro_table_functions[ro_table_id]() < ro_offset + len ) {
    unsafe_io( "Cannot copy from ro table to rw table. Ro out of bounds." );
  }
  if ( size_rw_table_functions[ro_table_id]() < rw_offset + len ) {
    unsafe_io( "Cannot copy from ro table to rw table. Rw out of bounds." );
  }
  for ( int i = 0; i < len; i++ ) {
    externref val = get_ro_table_functions[ro_table_id]( ro_offset + i );
    set_rw_table_functions[rw_table_id]( rw_offset + i, val );
  }
}