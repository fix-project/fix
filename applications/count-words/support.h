#include <stdint.h>

extern void ro_mem_0_to_program_mem( void* dst, uint32_t src, uint32_t len )
  __attribute__( ( import_module( "support" ), import_name( "ro_mem_0_to_program_mem" ) ) );
extern void program_mem_to_rw_mem_0( uint32_t dst, void* src, uint32_t len )
  __attribute__( ( import_module( "support" ), import_name( "program_mem_to_rw_mem_0" ) ) );

extern void ro_mem_1_to_program_mem( void* dst, uint32_t src, uint32_t len )
  __attribute__( ( import_module( "support" ), import_name( "ro_mem_1_to_program_mem" ) ) );
extern void program_mem_to_rw_mem_1( uint32_t dst, void* src, uint32_t len )
  __attribute__( ( import_module( "support" ), import_name( "program_mem_to_rw_mem_1" ) ) );
