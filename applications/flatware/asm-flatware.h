#include "../util/fixpoint_util.h"
#include "api.h"

enum RO_TABLE_ID
{
  InputROTable,
  ArgsROTable,
  ScratchROTable0, // Free to use as scratch space
  WorkingDirROTable 
  // The remaining RO tables are used as more file descriptors.
};
enum RW_TABLE_ID
{
  ReturnRWTable
};
enum RW_MEM_ID
{
  ScratchRWMem0, 
  StdOutRWMem,
  TraceRWMem
};
enum RO_MEM_ID
{
  ArgROMem,
  FileSystemROMem,
  FileSystemDataROMem,
  ScratchROMem0 // Free to use as scratch space
};

// Program mem
extern void memory_copy_program( int32_t, const void*, int32_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "memory_copy_program" ) ) );
extern void program_memory_to_flatware_memory( const void*, int32_t, int32_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "program_memory_to_flatware_memory" ) ) );
extern int32_t get_program_i32( int32_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "get_i32_program" ) ) );
extern void set_program_i32( int32_t, int32_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "set_i32_program" ) ) );
extern int32_t get_program_i8( int32_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "get_i8_program" ) ) );
extern void set_program_i8( int32_t, int32_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "set_i8_program" ) ) );

// Flatware mem
extern int32_t get_flatware_i32( const char* flatware_pointer)
  __attribute( ( import_module( "asm-flatware" ), import_name( "get_i32_flatware" ) ) );
extern void set_flatware_i32( const char* flatware_pointer, int32_t val )
  __attribute( ( import_module( "asm-flatware" ), import_name( "set_i32_flatware" ) ) );
extern int32_t get_flatware_i8( const char* flatware_pointer )
  __attribute( ( import_module( "asm-flatware" ), import_name( "get_i8_flatware" ) ) );
extern void set_flatware_i8( const char* flatware_pointer, int32_t val )
  __attribute( ( import_module( "asm-flatware" ), import_name( "set_i8_flatware" ) ) );

extern void run_start( void ) __attribute( ( import_module( "asm-flatware" ), import_name( "run-start" ) ) );
__attribute( ( noreturn ) ) void flatware_exit( void )
__attribute( ( import_module( "asm-flatware" ), import_name( "exit" ) ) );

void flatware_mem_to_rw_mem( int32_t rw_mem_id, int32_t rw_offset, const char* flatware_pointer, int32_t len );

void program_mem_to_rw_mem( int32_t rw_mem_id, int32_t rw_offset, int32_t program_offset, int32_t len );

void ro_mem_to_program_mem( int32_t ro_mem_id, int32_t program_offset, int32_t ro_offset, int32_t len );

void ro_mem_to_flatware_mem( int32_t ro_mem_id, const char* flatware_pointer, int32_t ro_offset, int32_t len );
