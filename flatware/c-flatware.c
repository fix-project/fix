#include "c-flatware.h"
#include "api.h"

#include <stdbool.h>

typedef char __attribute__( ( address_space( 10 ) ) ) * externref;

typedef struct filedesc
{
  int32_t offset;
  bool open;
  char pad[3];
} filedesc;

enum
{
  STDIN,
  STDOUT,
  STDERR,
  WORKINGDIR
};

static filedesc fds[16] = {
  { .open = true, .offset = 0 }, // STDIN
  { .open = true, .offset = 0 }, // STDOUT
  { .open = true, .offset = 0 }, // STDERR
  { .open = true, .offset = 0 }, // WORKINGDIR
};

extern void memory_copy_rw_0( const void*, int32_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "memory_copy_rw_0" ) ) );
extern void memory_copy_rw_1( const void*, int32_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "memory_copy_rw_1" ) ) );
extern void memory_copy_program( int32_t, const void*, int32_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "memory_copy_program" ) ) );
extern void program_memory_copy_rw_1( int32_t, int32_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "program_memory_copy_rw_1" ) ) );
extern void memory_copy_program_ro_0( int32_t, int32_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "memory_copy_program_ro_0" ) ) );
extern void memory_copy_program_ro_1( int32_t, int32_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "memory_copy_program_ro_1" ) ) );
extern externref create_blob_rw_mem_0( int32_t )
  __attribute( ( import_module( "fixpoint" ), import_name( "create_blob_rw_mem_0" ) ) );
extern externref create_blob_rw_mem_1( int32_t )
  __attribute( ( import_module( "fixpoint" ), import_name( "create_blob_rw_mem_1" ) ) );
extern externref create_tree_rw_table_0( int32_t )
  __attribute( ( import_module( "fixpoint" ), import_name( "create_tree_rw_table_0" ) ) );
extern void attach_tree_ro_table_0( externref )
  __attribute( ( import_module( "fixpoint" ), import_name( "attach_tree_ro_table_0" ) ) );
extern void attach_blob_ro_mem_0( externref )
  __attribute( ( import_module( "fixpoint" ), import_name( "attach_blob_ro_mem_0" ) ) );
extern void attach_blob_ro_mem_1( externref )
  __attribute( ( import_module( "fixpoint" ), import_name( "attach_blob_ro_mem_1" ) ) );
extern int32_t size_ro_mem_0( void ) __attribute( ( import_module( "fixpoint" ), import_name( "size_ro_mem_0" ) ) );
extern externref create_blob_i32( int32_t )
  __attribute( ( import_module( "fixpoint" ), import_name( "create_blob_i32" ) ) );

extern int32_t get_program_i32( int32_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "get_program_i32" ) ) );
extern void set_program_i32( int32_t, int32_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "set_program_i32" ) ) );
extern int32_t args_num( void ) __attribute( ( import_module( "asm-flatware" ), import_name( "args_num" ) ) );
extern externref get_arg( int32_t ) __attribute( ( import_module( "asm-flatware" ), import_name( "get_arg" ) ) );
extern int32_t arg_size( void ) __attribute( ( import_module( "asm-flatware" ), import_name( "arg_size" ) ) );

extern void run_start( void ) __attribute( ( import_module( "asm-flatware" ), import_name( "run-start" ) ) );
extern void set_return( int32_t, externref )
  __attribute( ( import_module( "asm-flatware" ), import_name( "set_return" ) ) );
__attribute( ( noreturn ) ) void flatware_exit( void )
  __attribute( ( import_module( "asm-flatware" ), import_name( "exit" ) ) );

externref fixpoint_apply( externref encode ) __attribute( ( export_name( "_fixpoint_apply" ) ) );

_Noreturn void proc_exit( int32_t rval )
{
  set_return( 0, create_blob_i32( rval ) );
  flatware_exit();
}

int32_t fd_close( __attribute__( ( unused ) ) int32_t fd )
{

  return 0;
}

int32_t fd_fdstat_get( __attribute__( ( unused ) ) int32_t fd, __attribute__( ( unused ) ) int32_t retptr0 )
{
  return 0;
}

int32_t fd_seek( __attribute__( ( unused ) ) int32_t fd,
                 __attribute__( ( unused ) ) int64_t offset,
                 __attribute__( ( unused ) ) int32_t whence,
                 __attribute__( ( unused ) ) int32_t retptr0 )
{
  return 0;
}
// fd_read copies from memory (ro mem 1) to program memory
// need to match signature from wasi-libc
int32_t fd_read( __attribute__( ( unused ) ) int32_t fd,
                 int32_t iovs,
                 __attribute__( ( unused ) ) int32_t iovs_len,
                 __attribute__( ( unused ) ) int32_t retptr0 )
{
  int32_t offset = get_program_i32( iovs );
  int32_t size = get_program_i32( iovs + 4 );
  memory_copy_program_ro_1( offset, size );
  fds[STDOUT].offset = size; // these fds need to be initialized to zero, then change to fds[1].offset +=size
  return size;
}

int32_t fd_write( __attribute__( ( unused ) ) int32_t fd,
                  int32_t iovs,
                  __attribute__( ( unused ) ) int32_t iovs_len,
                  __attribute__( ( unused ) ) int32_t retptr0 )
{
  int32_t offset = get_program_i32( iovs );
  int32_t size = get_program_i32( iovs + 4 );
  program_memory_copy_rw_1( offset, size );
  fds[STDOUT].offset += size;
  return size;
}

int32_t fd_fdstat_set_flags( __attribute__( ( unused ) ) int32_t fd, __attribute__( ( unused ) ) int32_t fdflags )
{
  return 0;
}

/**
 * Collect information of preopened file descriptors. Called with fd >= 3 (0, 1, 2 reserved for stdin, stdout and
 * stderror). Return BADF if fd is not preopened.
 *
 * working directory preopened at fd = 3
 */
int32_t fd_prestat_get( int32_t fd, int32_t retptr0 )
{
  // STDIN, STDOUT, STDERR
  if ( fd < 3 ) {
    return __WASI_ERRNO_PERM;
  }

  // file descriptors not preopened
  if ( fd > 3 ) {
    return __WASI_ERRNO_BADF;
  }

  // retptr offset in program memory where struct should be stored
  // TODO: fd == 3: working directory

  if ( fd == WORKINGDIR ) {
    __wasi_prestat_t ps;
    ps.tag = __WASI_PREOPENTYPE_DIR;
    ps.u.dir.pr_name_len = 2;

    memory_copy_program( retptr0, &ps, sizeof( ps ) );

    return 0;
  }
  return __WASI_ERRNO_BADF;
}

int32_t fd_prestat_dir_name( int32_t fd, int32_t path, int32_t path_len )
{
  char str[] = ".";

  if ( fd != WORKINGDIR ) {
    return __WASI_ERRNO_PERM;
  }

  memory_copy_program( path, str, path_len );
  return 0;
}

int32_t args_sizes_get( int32_t num_argument_ptr, int32_t size_argument_ptr )
{
  int32_t num = args_num() - 2;
  int32_t size = 0;
  memory_copy_program( num_argument_ptr, &num, 4 );

  // Actual arguments
  for ( int32_t i = 2; i < args_num(); i++ ) {
    attach_blob_ro_mem_0( get_arg( i ) );
    size += size_ro_mem_0();
  }

  memory_copy_program( size_argument_ptr, &size, 4 );
  return 0;
}

int32_t args_get( int32_t argv_ptr, int32_t argv_buf_ptr )
{
  int32_t size;
  int32_t addr = argv_buf_ptr;

  for ( int32_t i = 2; i < args_num(); i++ ) {
    attach_blob_ro_mem_0( get_arg( i ) );
    size = size_ro_mem_0();
    memory_copy_program( argv_ptr + ( i - 2 ) * 4, &addr, 4 );
    memory_copy_program_ro_0( addr, size );
    addr += size;
  }
  return 0;
}

int32_t path_open( __attribute__( ( unused ) ) int32_t fd,
                   __attribute__( ( unused ) ) int32_t path,
                   __attribute__( ( unused ) ) int32_t arg3,
                   __attribute__( ( unused ) ) int32_t arg4,
                   __attribute__( ( unused ) ) int32_t arg5,
                   __attribute__( ( unused ) ) int64_t arg6,
                   __attribute__( ( unused ) ) int64_t arg7,
                   __attribute__( ( unused ) ) int32_t arg8,
                   __attribute__( ( unused ) ) int32_t arg9 )
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

  set_return( 1, create_blob_rw_mem_1( fds[1].offset ) );

  return create_tree_rw_table_0( 2 );
}
