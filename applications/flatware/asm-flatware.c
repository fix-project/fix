#include "asm-flatware.h"

void flatware_mem_to_rw_mem( int32_t rw_mem_id, int32_t rw_offset, const char* flatware_pointer, int32_t len )
{
  for ( int i = 0; i < len; i ++ ) {
    int32_t val = get_flatware_i8( &flatware_pointer[i] );
    set_i8_rw_mem_functions[rw_mem_id]( rw_offset + i, val );
  }
}

void program_mem_to_rw_mem( int32_t rw_mem_id, int32_t rw_offset, int32_t program_offset, int32_t len )
{
  for ( int i = 0; i < len; i ++ ) {
    int32_t val = get_program_i8( program_offset + i );
    set_i8_rw_mem_functions[rw_mem_id]( rw_offset + i, val );
  }
}

void ro_mem_to_program_mem( int32_t ro_mem_id, int32_t program_offset, int32_t ro_offset, int32_t len )
{
  for ( int i = 0; i < len; i ++ ) {
    int32_t val = get_i8_ro_mem_functions[ro_mem_id]( ro_offset + i );
    set_program_i8( program_offset + i, val );
  }
}

void ro_mem_to_flatware_mem( int32_t ro_mem_id, const char* flatware_pointer, int32_t ro_offset, int32_t len )
{
  for ( int i = 0; i < len; i ++ ) {
    int32_t val = get_i8_ro_mem_functions[ro_mem_id]( ro_offset + i );
    set_flatware_i8( &flatware_pointer[i], val );
  }
}

void rw_mem_to_program_mem( int32_t rw_mem_id, int32_t program_offset, int32_t rw_offset, int32_t len )
{
  for ( int i = 0; i < len; i ++ ) {
    int32_t val = get_i8_rw_mem_functions[rw_mem_id]( rw_offset + i );
    set_program_i8( program_offset + i, val );
  }
}

void rw_mem_to_flatware_mem( int32_t rw_mem_id, const char* flatware_pointer, int32_t rw_offset, int32_t len )
{
  for ( int i = 0; i < len; i ++ ) {
    int32_t val = get_i8_rw_mem_functions[rw_mem_id]( rw_offset + i );
    set_flatware_i8( &flatware_pointer[i], val );
  }
}
