#include <stdint.h>

extern void fixpoint_unsafe_io( const char* s, int32_t size )
  __attribute__( ( import_module( "fixpoint" ), import_name( "memory_unsafe_io" ) ) );

extern void ro_mem_0_to_program_mem( void* dst, uint32_t src, uint32_t len )
  __attribute__( ( import_module( "support" ), import_name( "ro_mem_0_to_program_mem" ) ) );
extern void program_mem_to_rw_mem_0( uint32_t dst, void* src, uint32_t len )
  __attribute__( ( import_module( "support" ), import_name( "program_mem_to_rw_mem_0" ) ) );
extern void ro_mem_1_to_program_mem( void* dst, uint32_t src, uint32_t len )
  __attribute__( ( import_module( "support" ), import_name( "ro_mem_1_to_program_mem" ) ) );
externref get_ro_table_0( int32_t index )
  __attribute__( ( import_module( "support" ), import_name( "get_ro_table_0" ) ) );
int32_t grow_rw_mem_0_pages( int32_t size )
  __attribute__( ( import_module( "support" ), import_name( "grow_rw_mem_0" ) ) );
