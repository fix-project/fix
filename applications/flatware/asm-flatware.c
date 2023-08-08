#include "asm-flatware.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"

// copy flatware to rw mem
extern void flatware_memory_to_rw_mem_0( int32_t rw_offset, const void* flatware_pointer, int32_t len )
  __attribute( ( import_module( "asm-flatware" ), import_name( "flatware_memory_to_rw_0" ) ) );
extern void flatware_memory_to_rw_mem_1( int32_t rw_offset, const void* flatware_pointer, int32_t len )
  __attribute( ( import_module( "asm-flatware" ), import_name( "flatware_memory_to_rw_1" ) ) );
extern void flatware_memory_to_rw_mem_2( int32_t rw_offset, const void* flatware_pointer, int32_t len )
  __attribute( ( import_module( "asm-flatware" ), import_name( "flatware_memory_to_rw_2" ) ) );
static void (*const flatware_to_rw_functions[]) (int32_t, const void* ,int32_t) = {flatware_memory_to_rw_mem_0,flatware_memory_to_rw_mem_1,flatware_memory_to_rw_mem_2};

// copy program to rw
extern void program_memory_to_rw_mem_0( int32_t rw_offset, int32_t program_offset, int32_t len )
  __attribute( ( import_module( "asm-flatware" ), import_name( "program_memory_to_rw_0" ) ) );
extern void program_memory_to_rw_mem_1( int32_t rw_offset, int32_t program_offset, int32_t len )
  __attribute( ( import_module( "asm-flatware" ), import_name( "program_memory_to_rw_1" ) ) );
extern void program_memory_to_rw_mem_2( int32_t rw_offset, int32_t program_offset, int32_t len )
  __attribute( ( import_module( "asm-flatware" ), import_name( "program_memory_to_rw_2" ) ) );
static void (*const program_to_rw_functions[]) (int32_t, int32_t, int32_t) = {program_memory_to_rw_mem_0,program_memory_to_rw_mem_1,program_memory_to_rw_mem_2};

// copy ro mem to flatware
extern void ro_mem_0_to_flatware_memory( const void* flatware_pointer, int32_t ro_offset, int32_t len )
  __attribute( ( import_module( "asm-flatware" ), import_name( "ro_mem_0_to_flatware_memory" ) ) );
extern void ro_mem_1_to_flatware_memory( const void* flatware_pointer, int32_t ro_offset, int32_t len )
  __attribute( ( import_module( "asm-flatware" ), import_name( "ro_mem_1_to_flatware_memory" ) ) );
extern void ro_mem_2_to_flatware_memory( const void* flatware_pointer, int32_t ro_offset, int32_t len )
  __attribute( ( import_module( "asm-flatware" ), import_name( "ro_mem_2_to_flatware_memory" ) ) );
extern void ro_mem_3_to_flatware_memory( const void* flatware_pointer, int32_t ro_offset, int32_t len )
  __attribute( ( import_module( "asm-flatware" ), import_name( "ro_mem_3_to_flatware_memory" ) ) );
static void (*const ro_to_flatware_functions[]) (const void*, int32_t, int32_t) = {ro_mem_0_to_flatware_memory,ro_mem_1_to_flatware_memory,ro_mem_2_to_flatware_memory, ro_mem_3_to_flatware_memory};

// copy ro to program
extern void ro_mem_0_to_program_memory( int32_t program_offset, int32_t ro_offset, int32_t len )
  __attribute( ( import_module( "asm-flatware" ), import_name( "ro_mem_0_to_program_memory" ) ) );
extern void ro_mem_1_to_program_memory( int32_t program_offset, int32_t ro_offset, int32_t len )
  __attribute( ( import_module( "asm-flatware" ), import_name( "ro_mem_1_to_program_memory" ) ) );
extern void ro_mem_2_to_program_memory( int32_t program_offset, int32_t ro_offset, int32_t len )
  __attribute( ( import_module( "asm-flatware" ), import_name( "ro_mem_2_to_program_memory" ) ) );
extern void ro_mem_3_to_program_memory( int32_t program_offset, int32_t ro_offset, int32_t len )
  __attribute( ( import_module( "asm-flatware" ), import_name( "ro_mem_3_to_program_memory" ) ) );
static void (*const ro_to_program_functions[]) (int32_t, int32_t, int32_t) = {ro_mem_0_to_program_memory,ro_mem_1_to_program_memory,ro_mem_2_to_program_memory, ro_mem_3_to_program_memory};

void flatware_mem_to_rw_mem( int32_t rw_mem_id, int32_t rw_offset, const char* flatware_pointer, int32_t len )
{
  flatware_to_rw_functions[rw_mem_id](rw_offset, flatware_pointer, len);
}

void program_mem_to_rw_mem( int32_t rw_mem_id, int32_t rw_offset, int32_t program_offset, int32_t len )
{
  program_to_rw_functions[rw_mem_id](rw_offset, program_offset, len);
}

void ro_mem_to_program_mem( int32_t ro_mem_id, int32_t program_offset, int32_t ro_offset, int32_t len )
{
  ro_to_program_functions[ro_mem_id](program_offset,ro_offset, len);
}

void ro_mem_to_flatware_mem( int32_t ro_mem_id, const char* flatware_pointer, int32_t ro_offset, int32_t len )
{
  ro_to_flatware_functions[ro_mem_id](flatware_pointer,ro_offset, len);
}

#pragma clang diagnostic pop
