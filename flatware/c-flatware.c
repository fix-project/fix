#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef char __attribute__( ( address_space( 10 ) ) ) * externref;

typedef struct filedesc
{
  uint32_t offset;
} filedesc;

static struct filedesc stdout = { .offset = 0 };

static filedesc fds[16];

extern void memory_copy_rw_0( const void*, size_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "memory_copy_rw_0" ) ) );
extern void memory_copy_rw_1( const void*, size_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "memory_copy_rw_1" ) ) );
extern void memory_copy_program( uint32_t, const void*, size_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "memory_copy_program" ) ) );
extern void program_memory_copy_rw_1( uint32_t, uint32_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "program_memory_copy_rw_1" ) ) );
extern void memory_copy_program_ro_0( uint32_t, uint32_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "memory_copy_program_ro_0" ) ) );
extern void memory_copy_program_ro_1( uint32_t, uint32_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "memory_copy_program_ro_1" ) ) );
extern externref create_blob_rw_mem_0( uint32_t )
  __attribute( ( import_module( "fixpoint" ), import_name( "create_blob_rw_mem_0" ) ) );
extern externref create_blob_rw_mem_1( uint32_t )
  __attribute( ( import_module( "fixpoint" ), import_name( "create_blob_rw_mem_1" ) ) );
extern externref create_tree_rw_table_0( uint32_t )
  __attribute( ( import_module( "fixpoint" ), import_name( "create_tree_rw_table_0" ) ) );
extern void attach_tree_ro_table_0( externref )
  __attribute( ( import_module( "fixpoint" ), import_name( "attach_tree_ro_table_0" ) ) );
extern void attach_blob_ro_mem_0( externref )
  __attribute( ( import_module( "fixpoint" ), import_name( "attach_blob_ro_mem_0" ) ) );
extern void attach_blob_ro_mem_1( externref )
  __attribute( ( import_module( "fixpoint" ), import_name( "attach_blob_ro_mem_1" ) ) );
extern uint32_t size_ro_mem_0( void )
  __attribute( ( import_module( "fixpoint" ), import_name( "size_ro_mem_0" ) ) );
extern externref create_blob_i32( uint32_t )
  __attribute( ( import_module( "fixpoint" ), import_name( "create_blob_i32" ) ) );

extern uint32_t get_program_i32( uint32_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "get_program_i32" ) ) );
extern void set_program_i32( uint32_t, uint32_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "set_program_i32" ) ) );
extern uint32_t args_num( void ) __attribute( ( import_module( "asm-flatware" ), import_name( "args_num" ) ) );
extern externref get_arg( uint32_t ) __attribute( ( import_module( "asm-flatware" ), import_name( "get_arg" ) ) );
extern uint32_t arg_size( void ) __attribute( ( import_module( "asm-flatware" ), import_name( "arg_size" ) ) );

extern void run_start( void ) __attribute( ( import_module( "asm-flatware" ), import_name( "run-start" ) ) );
extern void set_return( uint32_t, externref )
  __attribute( ( import_module( "asm-flatware" ), import_name( "set_return" ) ) );
__attribute( ( noreturn ) ) void flatware_exit( void )
  __attribute( ( import_module( "asm-flatware" ), import_name( "exit" ) ) );

__attribute( ( noreturn ) ) void proc_exit( uint32_t rval ) __attribute( ( export_name( "proc_exit" ) ) );
uint32_t fd_close( int fd ) __attribute( ( export_name( "fd_close" ) ) );
uint32_t fd_fdstat_get( uint32_t fd, uint32_t res ) __attribute( ( export_name( "fd_fdstat_get" ) ) );
uint32_t fd_seek( uint32_t fd, int64_t offset, uint32_t whence, uint32_t file_size )
  __attribute( ( export_name( "fd_seek" ) ) ); // not used atm
uint32_t fd_write( uint32_t fd, uint32_t vec, uint32_t vec_len, uint32_t retptr0 )
  __attribute( ( export_name( "fd_write" ) ) );
uint32_t fd_fdstat_set_flags( uint32_t fd, uint32_t retptr0 )
  __attribute( ( export_name( "fd_fdstat_set_flags" ) ) );
uint32_t fd_prestat_dir_name( uint32_t arg0, uint32_t arg1, uint32_t arg2 )
  __attribute( ( export_name( "fd_prestat_dir_name" ) ) );
uint32_t fd_prestat_get( uint32_t fd, uint32_t retptr0 ) __attribute( ( export_name( "fd_prestat_get" ) ) );
uint32_t args_sizes_get( uint32_t num_argument_ptr, uint32_t size_argument_ptr )
  __attribute( ( export_name( "args_sizes_get" ) ) );
uint32_t args_get( uint32_t argv_ptr, uint32_t argv_buf_ptr ) __attribute( ( export_name( "args_get" ) ) );
uint32_t path_open( uint32_t fd,
                    uint32_t path,
                    uint32_t arg3,
                    uint32_t arg4,
                    uint32_t arg5,
                    uint64_t arg6,
                    uint64_t arg7,
                    uint32_t arg8,
                    uint32_t arg9 ) __attribute( ( export_name( "path_open" ) ) );
externref fixpoint_apply( externref encode ) __attribute( ( export_name( "_fixpoint_apply" ) ) );
uint32_t fd_read( uint32_t fd, uint32_t iovs, uint32_t len, uint32_t retptr0 )
  __attribute( ( export_name( "fd_read" ) ) );

__attribute( ( noreturn ) ) void proc_exit( uint32_t rval )
{
  set_return( 0, create_blob_i32( rval ) );
  flatware_exit();
}

uint32_t fd_close( __attribute__( ( unused ) ) int fd )
{

  return 0;
}

uint32_t fd_fdstat_get( __attribute__( ( unused ) ) uint32_t fd, __attribute__( ( unused ) ) uint32_t res )
{
  return 0;
}

uint32_t fd_seek( __attribute__( ( unused ) ) uint32_t fd,
                  __attribute__( ( unused ) ) int64_t offset,
                  __attribute__( ( unused ) ) uint32_t whence,
                  __attribute__( ( unused ) ) uint32_t file_size )
{
  return 0;
}
// fd_read copies from memory (ro mem 1) to program memory
// need to match signature from wasi-libc
uint32_t fd_read( __attribute__( ( unused ) ) uint32_t fd,
                  uint32_t iovs,
                  __attribute__( ( unused ) ) uint32_t iovs_len,
                  __attribute__( ( unused ) ) uint32_t retptr0 )
{
  uint32_t offset = get_program_i32( iovs );
  uint32_t size = get_program_i32( iovs + 4 );
  memory_copy_program_ro_1( offset, size );
  fds[1].offset = size; // these fds need to be initialized to zero, then change to fds[1].offset +=size
  return size;
}

uint32_t fd_write( __attribute__( ( unused ) ) uint32_t fd,
                   uint32_t iovs,
                   __attribute__( ( unused ) ) uint32_t iovs_len,
                   __attribute__( ( unused ) ) uint32_t retptr0 )
{
  uint32_t offset = get_program_i32( iovs );
  uint32_t size = get_program_i32( iovs + 4 );
  program_memory_copy_rw_1( offset, size );
  stdout.offset += size;
  return size;
}

uint32_t fd_fdstat_set_flags( __attribute__( ( unused ) ) uint32_t fd,
                              __attribute__( ( unused ) ) uint32_t fdflags )
{
  return 0;
}

uint32_t fd_prestat_get( __attribute__( ( unused ) ) uint32_t fd, __attribute__( ( unused ) ) uint32_t retptr0 )
{
  return 0;
}

uint32_t fd_prestat_dir_name( __attribute__( ( unused ) ) uint32_t arg0,
                              __attribute__( ( unused ) ) uint32_t arg1,
                              __attribute__( ( unused ) ) uint32_t arg2 )
{
  return 0;
}

uint32_t args_sizes_get( uint32_t num_argument_ptr, uint32_t size_argument_ptr )
{
  uint32_t num = args_num() - 2;
  uint32_t size = 0;
  memory_copy_program( num_argument_ptr, &num, 4 );

  // Actual arguments
  for ( uint32_t i = 2; i < args_num(); i++ ) {
    attach_blob_ro_mem_0( get_arg( i ) );
    size += size_ro_mem_0();
  }

  memory_copy_program( size_argument_ptr, &size, 4 );
  return 0;
}

uint32_t args_get( uint32_t argv_ptr, uint32_t argv_buf_ptr )
{
  uint32_t size;
  uint32_t addr = argv_buf_ptr;

  for ( uint32_t i = 2; i < args_num(); i++ ) {
    attach_blob_ro_mem_0( get_arg( i ) );
    size = size_ro_mem_0();
    memory_copy_program( argv_ptr + ( i - 2 ) * 4, &addr, 4 );
    memory_copy_program_ro_0( addr, size );
    addr += size;
  }
  return 0;
}

uint32_t path_open( __attribute__( ( unused ) ) uint32_t fd,
                    __attribute__( ( unused ) ) uint32_t path,
                    __attribute__( ( unused ) ) uint32_t arg3,
                    __attribute__( ( unused ) ) uint32_t arg4,
                    __attribute__( ( unused ) ) uint32_t arg5,
                    __attribute__( ( unused ) ) uint64_t arg6,
                    __attribute__( ( unused ) ) uint64_t arg7,
                    __attribute__( ( unused ) ) uint32_t arg8,
                    __attribute__( ( unused ) ) uint32_t arg9 )
{
  externref fs = get_arg( 2 );

  attach_blob_ro_mem_1( fs );

  return 1;
}

externref fixpoint_apply( externref encode )
{
  set_return( 0, create_blob_i32( 0 ) );

  attach_tree_ro_table_0( encode );

  run_start();

  set_return( 1, create_blob_rw_mem_1( stdout.offset ) );

  return create_tree_rw_table_0( 2 );
}
