#include "c-flatware.h"
#include "api.h"
#include "asm-flatware.h"
#include "filesys.h"

#include <stdbool.h>

typedef char __attribute__( ( address_space( 10 ) ) ) * externref;

// XXX make sure this is false before committing
static bool trace = false;
static int32_t trace_offset = 0;

#define TRACE()                                                                                                    \
  if ( trace )                                                                                                     \
  print_trace( __FUNCTION__ )

static void print_trace( const char* f_name )
{
  static const char newline = '\n';
  int32_t f_name_len = 0;

  const char* tmp = f_name;
  while ( *tmp ) {
    f_name_len++;
    tmp++;
  }

  flatware_memory_to_rw_2( trace_offset, &newline, 1 );
  trace_offset++;

  flatware_memory_to_rw_2( trace_offset, f_name, f_name_len );
  trace_offset += f_name_len;
}

typedef struct filedesc
{
  int32_t offset;
  int32_t size;
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
  { .open = true, .size = -1, .offset = 0 }, // STDIN
  { .open = true, .size = -1, .offset = 0 }, // STDOUT
  { .open = true, .size = -1, .offset = 0 }, // STDERR
  { .open = true, .size = -1, .offset = 0 }, // WORKINGDIR
};

extern void memory_copy_program( int32_t, const void*, int32_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "memory_copy_program" ) ) );
extern int32_t get_program_i32( int32_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "get_program_i32" ) ) );
extern void set_program_i32( int32_t, int32_t )
  __attribute( ( import_module( "asm-flatware" ), import_name( "set_program_i32" ) ) );

extern void run_start( void ) __attribute( ( import_module( "asm-flatware" ), import_name( "run-start" ) ) );
__attribute( ( noreturn ) ) void flatware_exit( void )
  __attribute( ( import_module( "asm-flatware" ), import_name( "exit" ) ) );

externref fixpoint_apply( externref encode ) __attribute( ( export_name( "_fixpoint_apply" ) ) );

_Noreturn void proc_exit( int32_t rval )
{
  TRACE();
  set_rw_table_0( 0, create_blob_i32( rval ) );
  flatware_exit();
}

int32_t fd_close( __attribute__( ( unused ) ) int32_t fd )
{
  TRACE();
  return 0;
}

int32_t fd_fdstat_get( __attribute__( ( unused ) ) int32_t fd, __attribute__( ( unused ) ) int32_t retptr0 )
{
  TRACE();
  return 0;
}

int32_t fd_seek( __attribute__( ( unused ) ) int32_t fd,
                 __attribute__( ( unused ) ) int64_t offset,
                 __attribute__( ( unused ) ) int32_t whence,
                 __attribute__( ( unused ) ) int32_t retptr0 )
{
  TRACE();
  return 0;
}

// fd_read copies from memory (ro mem 1) to program memory
// need to match signature from wasi-libc
int32_t fd_read( int32_t fd, int32_t iovs, int32_t iovs_len, int32_t retptr0 )
{
  int32_t iobuf_offset, iobuf_len;
  int32_t file_remaining;
  int32_t size_to_read;
  int32_t total_read = 0;

  TRACE();

  if ( fd != 4 )
    return __WASI_ERRNO_BADF;

  for ( int32_t i = 0; i < iovs_len; i++ ) {
    file_remaining = fds[fd].size - fds[fd].offset;
    if ( file_remaining == 0 )
      break;

    iobuf_offset = get_program_i32( iovs + i * 8 );
    iobuf_len = get_program_i32( iovs + 4 + i * 8 );

    size_to_read = iobuf_len < file_remaining ? iobuf_len : file_remaining;
    ro_1_to_program_memory( iobuf_offset, fds[fd].offset, size_to_read );
    fds[fd].offset += size_to_read;
    total_read += size_to_read;
  }

  memory_copy_program( retptr0, &total_read, sizeof( total_read ) );
  return 0;
}

int32_t fd_write( int32_t fd, int32_t iovs, int32_t iovs_len, int32_t retptr0 )
{
  int32_t iobuf_offset, iobuf_len;
  int32_t total_written = 0;

  TRACE();

  if ( fd != STDOUT )
    return __WASI_ERRNO_BADF;

  for ( int32_t i = 0; i < iovs_len; i++ ) {
    iobuf_offset = get_program_i32( iovs + i * 8 );
    iobuf_len = get_program_i32( iovs + 4 + i * 8 );

    program_memory_to_rw_1( fds[STDOUT].offset, iobuf_offset, iobuf_len );
    fds[STDOUT].offset += iobuf_len;
    total_written += iobuf_len;
  }

  memory_copy_program( retptr0, &total_written, sizeof( total_written ) );
  return 0;
}

int32_t fd_fdstat_set_flags( __attribute__( ( unused ) ) int32_t fd, __attribute__( ( unused ) ) int32_t fdflags )
{
  TRACE();
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
  TRACE();

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

  TRACE();

  if ( fd != WORKINGDIR ) {
    return __WASI_ERRNO_PERM;
  }

  memory_copy_program( path, str, path_len );
  return 0;
}

int32_t fd_advise( __attribute__( ( unused ) ) int32_t fd,
                   __attribute__( ( unused ) ) int64_t offset,
                   __attribute__( ( unused ) ) int64_t len,
                   __attribute__( ( unused ) ) int32_t advice )
{
  TRACE();
  return 0;
}

int32_t fd_allocate( __attribute__( ( unused ) ) int32_t fd,
                     __attribute__( ( unused ) ) int64_t offset,
                     __attribute__( ( unused ) ) int64_t len )
{
  TRACE();
  return 0;
}

int32_t fd_datasync( __attribute__( ( unused ) ) int32_t fd )
{
  TRACE();
  return 0;
}

int32_t fd_filestat_get( __attribute__( ( unused ) ) int32_t fd, __attribute__( ( unused ) ) int32_t retptr0 )
{
  TRACE();
  return 0;
}

int32_t fd_filestat_set_size( __attribute__( ( unused ) ) int32_t fd, __attribute__( ( unused ) ) int64_t size )
{
  TRACE();
  return 0;
}

int32_t fd_filestat_set_times( __attribute__( ( unused ) ) int32_t fd,
                               __attribute__( ( unused ) ) int64_t atim,
                               __attribute__( ( unused ) ) int64_t mtim,
                               __attribute__( ( unused ) ) int32_t fst_flags )
{
  TRACE();
  return 0;
}

int32_t fd_pread( __attribute__( ( unused ) ) int32_t fd,
                  __attribute__( ( unused ) ) int32_t iovs,
                  __attribute__( ( unused ) ) int32_t iovs_len,
                  __attribute__( ( unused ) ) int64_t offset,
                  __attribute__( ( unused ) ) int32_t retptr0 )
{
  TRACE();
  return 0;
}

int32_t fd_pwrite( __attribute__( ( unused ) ) int32_t fd,
                   __attribute__( ( unused ) ) int32_t iovs,
                   __attribute__( ( unused ) ) int32_t iovs_len,
                   __attribute__( ( unused ) ) int64_t offset,
                   __attribute__( ( unused ) ) int32_t retptr0 )
{
  TRACE();
  return 0;
}

int32_t fd_readdir( __attribute__( ( unused ) ) int32_t fd,
                    __attribute__( ( unused ) ) int32_t buf,
                    __attribute__( ( unused ) ) int32_t buf_len,
                    __attribute__( ( unused ) ) int64_t cookie,
                    __attribute__( ( unused ) ) int32_t retptr0 )
{
  TRACE();
  return 0;
}

int32_t fd_sync( __attribute__( ( unused ) ) int32_t fd )
{
  TRACE();
  return 0;
}

int32_t fd_tell( __attribute__( ( unused ) ) int32_t fd, __attribute__( ( unused ) ) int32_t retptr0 )
{
  TRACE();
  return 0;
}

int32_t path_create_directory( __attribute__( ( unused ) ) int32_t fd,
                               __attribute__( ( unused ) ) int32_t path,
                               __attribute__( ( unused ) ) int32_t path_len )
{
  TRACE();
  return 0;
}

int32_t path_filestat_get( __attribute__( ( unused ) ) int32_t fd,
                           __attribute__( ( unused ) ) int32_t flags,
                           __attribute__( ( unused ) ) int32_t path,
                           __attribute__( ( unused ) ) int32_t path_len,
                           __attribute__( ( unused ) ) int32_t retptr0 )
{
  TRACE();
  return 0;
}

int32_t path_link( __attribute__( ( unused ) ) int32_t old_fd,
                   __attribute__( ( unused ) ) int32_t old_flags,
                   __attribute__( ( unused ) ) int32_t old_path,
                   __attribute__( ( unused ) ) int32_t old_path_len,
                   __attribute__( ( unused ) ) int32_t new_fd,
                   __attribute__( ( unused ) ) int32_t new_path,
                   __attribute__( ( unused ) ) int32_t new_path_len )
{
  TRACE();
  return 0;
}

int32_t path_readlink( __attribute__( ( unused ) ) int32_t fd,
                       __attribute__( ( unused ) ) int32_t path,
                       __attribute__( ( unused ) ) int32_t path_len,
                       __attribute__( ( unused ) ) int32_t buf,
                       __attribute__( ( unused ) ) int32_t buf_len,
                       __attribute__( ( unused ) ) int32_t retptr0 )
{
  TRACE();
  return 0;
}

int32_t path_remove_directory( __attribute__( ( unused ) ) int32_t fd,
                               __attribute__( ( unused ) ) int32_t path,
                               __attribute__( ( unused ) ) int32_t path_len )
{
  TRACE();
  return 0;
}

int32_t path_rename( __attribute__( ( unused ) ) int32_t fd,
                     __attribute__( ( unused ) ) int32_t old_path,
                     __attribute__( ( unused ) ) int32_t old_path_len,
                     __attribute__( ( unused ) ) int32_t new_fd,
                     __attribute__( ( unused ) ) int32_t new_path,
                     __attribute__( ( unused ) ) int32_t new_path_len )
{
  TRACE();
  return 0;
}

int32_t path_symlink( __attribute__( ( unused ) ) int32_t old_path,
                      __attribute__( ( unused ) ) int32_t old_path_len,
                      __attribute__( ( unused ) ) int32_t fd,
                      __attribute__( ( unused ) ) int32_t new_path,
                      __attribute__( ( unused ) ) int32_t new_path_len )
{
  TRACE();
  return 0;
}

int32_t path_unlink_file( __attribute__( ( unused ) ) int32_t fd,
                          __attribute__( ( unused ) ) int32_t path,
                          __attribute__( ( unused ) ) int32_t path_len )
{
  TRACE();
  return 0;
}

int32_t args_sizes_get( int32_t num_argument_ptr, int32_t size_argument_ptr )
{
  int32_t num = size_ro_table_0() - 2;
  int32_t size = 0;

  TRACE();

  memory_copy_program( num_argument_ptr, &num, 4 );

  // Actual arguments
  for ( int32_t i = 2; i < size_ro_table_0(); i++ ) {
    attach_blob_ro_mem_0( get_ro_table_0( i ) );
    size += size_ro_mem_0();
  }

  memory_copy_program( size_argument_ptr, &size, 4 );
  return 0;
}

int32_t args_get( int32_t argv_ptr, int32_t argv_buf_ptr )
{
  int32_t size;
  int32_t addr = argv_buf_ptr;

  TRACE();

  for ( int32_t i = 2; i < size_ro_table_0(); i++ ) {
    attach_blob_ro_mem_0( get_ro_table_0( i ) );
    size = size_ro_mem_0();
    memory_copy_program( argv_ptr + ( i - 2 ) * 4, &addr, 4 );
    ro_0_to_program_memory( addr, 0, size );
    addr += size;
  }

  return 0;
}

int32_t environ_sizes_get( __attribute__( ( unused ) ) int32_t retptr0,
                           __attribute__( ( unused ) ) int32_t retptr1 )
{
  TRACE();
  return 0;
}

int32_t environ_get( __attribute__( ( unused ) ) int32_t environ, __attribute__( ( unused ) ) int32_t environ_buf )
{
  TRACE();
  return 0;
}

int32_t path_open( __attribute__( ( unused ) ) int32_t fd,
                   __attribute__( ( unused ) ) int32_t dirflags,
                   __attribute__( ( unused ) ) int32_t path,
                   __attribute__( ( unused ) ) int32_t path_len,
                   __attribute__( ( unused ) ) int32_t oflags,
                   __attribute__( ( unused ) ) int64_t fs_rights_base,
                   __attribute__( ( unused ) ) int64_t fs_rights_inheriting,
                   __attribute__( ( unused ) ) int32_t fdflags,
                   int32_t retptr0 )
{
  __wasi_fd_t retfd;
  externref fs = get_ro_table_0( 2 );

  TRACE();

  attach_blob_ro_mem_1( fs );

  retfd = 4;
  fds[retfd].offset = 0;
  fds[retfd].size = size_ro_mem_1() - 1;
  fds[retfd].open = true;

  memory_copy_program( retptr0, &retfd, sizeof( retfd ) );
  return 0;
}

int32_t clock_res_get( __attribute__( ( unused ) ) int32_t id, __attribute__( ( unused ) ) int32_t retptr0 )
{
  TRACE();
  return 0;
}

int32_t clock_time_get( __attribute__( ( unused ) ) int32_t id,
                        __attribute__( ( unused ) ) int64_t precision,
                        __attribute__( ( unused ) ) int32_t retptr0 )
{
  TRACE();
  return 0;
}

int32_t poll_oneoff( __attribute__( ( unused ) ) int32_t in,
                     __attribute__( ( unused ) ) int32_t out,
                     __attribute__( ( unused ) ) int32_t nsubscriptions,
                     __attribute__( ( unused ) ) int32_t retptr0 )
{
  TRACE();
  return 0;
}

int32_t sched_yield( void )
{
  TRACE();
  return 0;
}

int32_t random_get( __attribute__( ( unused ) ) int32_t buf, __attribute__( ( unused ) ) int32_t buf_len )
{
  TRACE();
  return 0;
}

externref fixpoint_apply( externref encode )
{
  set_rw_table_0( 0, create_blob_i32( 0 ) );

  attach_tree_ro_table_0( encode );

  // TODO set `trace` here based on encode environment variables

  run_start();

  set_rw_table_0( 1, create_blob_rw_mem_1( fds[1].offset ) );

  set_rw_table_0( 2, create_blob_rw_mem_2( trace_offset ) );

  return create_tree_rw_table_0( 2 );
}

externref get_ro_table( int32_t table_index, int32_t index )
{
  return get_ro_table_functions[table_index]( index );
}
