#include "c-flatware.h"
#include "api.h"
#include "asm-flatware.h"
#include "filesys.h"

#include <stdarg.h>
#include <stdbool.h>

typedef char __attribute__( ( address_space( 10 ) ) ) * externref;

typedef struct filedesc
{
  int32_t offset;
  int32_t size;
  bool open;
  char pad[7];
  __wasi_fdstat_t stat;
} filedesc;

enum
{
  STDIN,
  STDOUT,
  STDERR,
  WORKINGDIR
};

#define N_FDS 16
static filedesc fds[N_FDS] = {
  { .open = true, .size = -1, .offset = 0 }, // STDIN
  { .open = true, .size = -1, .offset = 0 }, // STDOUT
  { .open = true, .size = -1, .offset = 0 }, // STDERR
  { .open = true, .size = -1, .offset = 0 }, // WORKINGDIR
};

#define DO_TRACE 0

// for each value, first pass the width (T32 or T64), then pass the value
// to specify the end, pass TEND
// this is important because we need to know the size of each value when getting it with `va_arg`
// e.g. FUNC_TRACE( T32, 0xdeadbeef, T64, 0xdeadbeeffeedface, TEND )
//      FUNC_TRACE( TEND ) for no values passed
#if DO_TRACE
#define FUNC_TRACE( ... ) print_trace( __FUNCTION__, __VA_ARGS__ )
#define RET_TRACE( val ) write_ret( val )
#define RAW_TRACE( str, n ) write_trace( str, n )
#define INT_TRACE( val ) write_int( val )
#else
#define FUNC_TRACE( ... ) dummy_trace( 0, __VA_ARGS__ )
#define RET_TRACE( val ) dummy_trace( 0, val )
#define RAW_TRACE( str, n ) dummy_trace( 0, str, n )
#define INT_TRACE( val ) dummy_trace( 0, val )
#endif

typedef enum trace_val_size
{
  TEND,
  T32,
  T64
} trace_val_size;

// must be <= 16
#define TRACE_RADIX 10

static int32_t strlen( const char* str )
{
  int32_t len = 0;
  while ( *str ) {
    len++;
    str++;
  }
  return len;
}

static void write_trace( const char* str, int32_t len )
{
  flatware_memory_to_rw_2( fds[STDERR].offset, str, len );
  fds[STDERR].offset += len;
}

static void write_uint( uint64_t val )
{
  const char nums[] = "0123456789abcdef";
  uint64_t div = 1;

  while ( val / div >= TRACE_RADIX ) {
    div *= TRACE_RADIX;
  }

  while ( div != 1 ) {
    write_trace( &nums[val / div], 1 );
    val %= div;
    div /= TRACE_RADIX;
  }
  write_trace( &nums[val], 1 );
}

#if DO_TRACE
#else
__attribute__( ( unused ) )
#endif
static void write_int( int64_t val )
{
  if ( val < 0 ) {
    write_trace( "-", 1 );
    val = -val;
  }
  write_uint( (uint64_t)val );
}

#if DO_TRACE
#else
__attribute__( ( unused ) )
#endif
static void write_ret( int64_t val )
{
  write_trace( " -> ", 4 );
  write_int( val );
}

#if DO_TRACE
#else
__attribute__( ( unused ) )
#endif
static void print_trace( const char* f_name, ... )
{
  const char chars[] = "\n( ), ";
  va_list vargs;
  trace_val_size v;

  // newline
  write_trace( &chars[0], 1 );

  // function name
  write_trace( f_name, strlen( f_name ) );

  // opening parenthesis
  write_trace( &chars[1], 2 );

  va_start( vargs, f_name );

  // passed values
  v = va_arg( vargs, trace_val_size );
  while ( 1 ) {
    int64_t val;

    if ( v == T32 )
      val = va_arg( vargs, int32_t );
    else if ( v == T64 )
      val = va_arg( vargs, int64_t );
    else
      break;

    write_int( val );

    v = va_arg( vargs, trace_val_size );
    if ( v != TEND ) {
      write_trace( &chars[4], 2 );
    }
  }

  va_end( vargs );

  // closing parenthesis
  write_trace( &chars[2], 2 );
}

#if DO_TRACE
__attribute__( ( unused ) )
#else
#endif
static void
dummy_trace( __attribute__( ( unused ) ) int reqd_arg, ... )
{
}

externref fixpoint_apply( externref encode ) __attribute( ( export_name( "_fixpoint_apply" ) ) );

_Noreturn void proc_exit( int32_t rval )
{
  FUNC_TRACE( T32, rval, TEND );

  set_rw_table_0( 0, create_blob_i32( rval ) );
  flatware_exit();
}

int32_t fd_close( int32_t fd )
{
  FUNC_TRACE( T32, fd, TEND );

  if ( fd <= 3 || fd >= N_FDS || fds[fd].open == false )
    return __WASI_ERRNO_BADF;

  fds[fd].open = false;

  return 0;
}

int32_t fd_fdstat_get( int32_t fd, int32_t retptr0 )
{
  __wasi_fdstat_t stat;

  FUNC_TRACE( T32, fd, T32, retptr0, TEND );

  if ( fd >= N_FDS || fds[fd].open == false ) {
    return __WASI_ERRNO_BADF;
  }

  if ( fd > 3 ) {
    memory_copy_program( retptr0, &fds[fd].stat, sizeof( fds[fd].stat ) );
    return 0;
  }

  stat.fs_filetype = __WASI_FILETYPE_UNKNOWN;
  stat.fs_flags = __WASI_FDFLAGS_APPEND;
  stat.fs_rights_base = __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_SEEK;
  if ( fd == STDOUT )
    stat.fs_rights_base |= __WASI_RIGHTS_FD_WRITE;
  stat.fs_rights_inheriting = stat.fs_rights_base;

  memory_copy_program( retptr0, &stat, sizeof( stat ) );
  return 0;
}

int32_t fd_seek( int32_t fd, int64_t offset, int32_t whence, int32_t retptr0 )
{
  FUNC_TRACE( T32, fd, T64, offset, T32, whence, T32, retptr0, TEND );

  if ( fd != 4 )
    return __WASI_ERRNO_BADF;

  if ( fds[fd].offset + offset + whence >= fds[fd].size ) {
    return __WASI_ERRNO_INVAL;
  }

  fds[fd].offset += offset + whence;

  memory_copy_program( retptr0, &fds[fd].offset, sizeof( fds[fd].offset ) );
  RET_TRACE( fds[fd].offset );
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

  FUNC_TRACE( T32, fd, T32, iovs, T32, iovs_len, T32, retptr0, TEND );

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
  RET_TRACE( total_read );
  return 0;
}

int32_t fd_write( int32_t fd, int32_t iovs, int32_t iovs_len, int32_t retptr0 )
{
  int32_t iobuf_offset, iobuf_len;
  int32_t total_written = 0;

  FUNC_TRACE( T32, fd, T32, iovs, T32, iovs_len, T32, retptr0, TEND );

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
  RET_TRACE( total_written );
  return 0;
}

int32_t fd_fdstat_set_flags( int32_t fd, int32_t fdflags )
{
  FUNC_TRACE( T32, fd, T32, fdflags, TEND );

  if ( fd <= 3 || fd >= N_FDS || fds[fd].open == false )
    return __WASI_ERRNO_BADF;

  if ( !( fds[fd].stat.fs_rights_base & __WASI_RIGHTS_FD_FDSTAT_SET_FLAGS ) )
    return __WASI_ERRNO_PERM;

  fds[fd].stat.fs_flags = (__wasi_fdflags_t)fdflags;
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
  FUNC_TRACE( T32, fd, T32, retptr0, TEND );

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

  FUNC_TRACE( T32, fd, T32, path, T32, path_len, TEND );

  if ( fd != WORKINGDIR ) {
    return __WASI_ERRNO_PERM;
  }

  memory_copy_program( path, str, path_len );
  return 0;
}

int32_t fd_advise( int32_t fd, int64_t offset, int64_t len, int32_t advice )
{
  FUNC_TRACE( T32, fd, T64, offset, T64, len, T32, advice, TEND );

  return 0;
}

int32_t fd_allocate( int32_t fd, int64_t offset, int64_t len )
{
  FUNC_TRACE( T32, fd, T64, offset, T64, len, TEND );

  return 0;
}

int32_t fd_datasync( int32_t fd )
{
  FUNC_TRACE( T32, fd, TEND );

  return 0;
}

int32_t fd_filestat_get( int32_t fd, int32_t retptr0 )
{
  FUNC_TRACE( T32, fd, T32, retptr0, TEND );

  return 0;
}

int32_t fd_filestat_set_size( int32_t fd, int64_t size )
{
  FUNC_TRACE( T32, fd, T64, size, TEND );

  return 0;
}

int32_t fd_filestat_set_times( int32_t fd, int64_t atim, int64_t mtim, int32_t fst_flags )
{
  FUNC_TRACE( T32, fd, T64, atim, T64, mtim, T32, fst_flags, TEND );

  return 0;
}

int32_t fd_pread( int32_t fd, int32_t iovs, int32_t iovs_len, int64_t offset, int32_t retptr0 )
{
  FUNC_TRACE( T32, fd, T32, iovs, T32, iovs_len, T64, offset, T32, retptr0, TEND );

  return 0;
}

int32_t fd_pwrite( int32_t fd, int32_t iovs, int32_t iovs_len, int64_t offset, int32_t retptr0 )
{
  FUNC_TRACE( T32, fd, T32, iovs, T32, iovs_len, T64, offset, T32, retptr0, TEND );

  return 0;
}

int32_t fd_readdir( int32_t fd, int32_t buf, int32_t buf_len, int64_t cookie, int32_t retptr0 )
{
  FUNC_TRACE( T32, fd, T32, buf, T32, buf_len, T64, cookie, T32, retptr0, TEND );

  return 0;
}

int32_t fd_sync( int32_t fd )
{
  FUNC_TRACE( T32, fd, TEND );

  return 0;
}

int32_t fd_tell( int32_t fd, int32_t retptr0 )
{
  FUNC_TRACE( T32, fd, T32, retptr0, TEND );

  return 0;
}

int32_t path_create_directory( int32_t fd, int32_t path, int32_t path_len )
{
  FUNC_TRACE( T32, fd, T32, path, T32, path_len, TEND );

  return 0;
}

int32_t path_filestat_get( int32_t fd, int32_t flags, int32_t path, int32_t path_len, int32_t retptr0 )
{
  FUNC_TRACE( T32, fd, T32, flags, T32, path, T32, path_len, T32, retptr0, TEND );

  return 0;
}

int32_t path_filestat_set_times( int32_t fd,
                                 int32_t flags,
                                 int32_t path,
                                 int32_t path_len,
                                 int64_t atim,
                                 int64_t mtim,
                                 int32_t fst_flags )
{
  FUNC_TRACE( T32, fd, T32, flags, T32, path, T32, path_len, T64, atim, T64, mtim, T32, fst_flags, TEND );

  return 0;
}

int32_t path_link( int32_t old_fd,
                   int32_t old_flags,
                   int32_t old_path,
                   int32_t old_path_len,
                   int32_t new_fd,
                   int32_t new_path,
                   int32_t new_path_len )
{
  FUNC_TRACE( T32,
              old_fd,
              T32,
              old_flags,
              T32,
              old_path,
              T32,
              old_path_len,
              T32,
              new_fd,
              T32,
              new_path,
              T32,
              new_path_len,
              TEND );
  return 0;
}

int32_t path_readlink( int32_t fd, int32_t path, int32_t path_len, int32_t buf, int32_t buf_len, int32_t retptr0 )
{
  FUNC_TRACE( T32, fd, T32, path, T32, path_len, T32, buf, T32, buf_len, T32, retptr0, TEND );

  return 0;
}

int32_t path_remove_directory( int32_t fd, int32_t path, int32_t path_len )
{
  FUNC_TRACE( T32, fd, T32, path, T32, path_len, TEND );

  return 0;
}

int32_t path_rename( int32_t fd,
                     int32_t old_path,
                     int32_t old_path_len,
                     int32_t new_fd,
                     int32_t new_path,
                     int32_t new_path_len )
{
  FUNC_TRACE( T32, fd, T32, old_path, T32, old_path_len, T32, new_fd, T32, new_path, T32, new_path_len, TEND );

  return 0;
}

int32_t path_symlink( int32_t old_path, int32_t old_path_len, int32_t fd, int32_t new_path, int32_t new_path_len )
{
  FUNC_TRACE( T32, old_path, T32, old_path_len, T32, fd, T32, new_path, T32, new_path_len, TEND );

  return 0;
}

int32_t path_unlink_file( int32_t fd, int32_t path, int32_t path_len )
{
  FUNC_TRACE( T32, fd, T32, path, T32, path_len, TEND );

  return 0;
}

int32_t args_sizes_get( int32_t num_argument_ptr, int32_t size_argument_ptr )
{
  int32_t num;
  int32_t size = 0;

  FUNC_TRACE( T32, num_argument_ptr, T32, size_argument_ptr, TEND );

  attach_tree_ro_table_1( get_ro_table_0( 2 ) );

  num = size_ro_table_1();

  memory_copy_program( num_argument_ptr, &num, 4 );
  RET_TRACE( num );

  // Actual arguments
  for ( int32_t i = 0; i < num; i++ ) {
    attach_blob_ro_mem_0( get_ro_table_1( i ) );
    size += size_ro_mem_0();
  }

  memory_copy_program( size_argument_ptr, &size, 4 );
  RAW_TRACE( ", ", 2 );
  INT_TRACE( size );
  return 0;
}

int32_t args_get( int32_t argv_ptr, int32_t argv_buf_ptr )
{
  int32_t size;
  int32_t addr = argv_buf_ptr;

  FUNC_TRACE( T32, argv_ptr, T32, argv_buf_ptr, TEND );

  attach_tree_ro_table_1( get_ro_table_0( 2 ) );

  for ( int32_t i = 0; i < size_ro_table_1(); i++ ) {
    attach_blob_ro_mem_0( get_ro_table_1( i ) );
    size = size_ro_mem_0();
    memory_copy_program( argv_ptr + i * 4, &addr, 4 );
    ro_0_to_program_memory( addr, 0, size );
    addr += size;
  }

  return 0;
}

int32_t environ_sizes_get( int32_t retptr0, int32_t retptr1 )
{
  FUNC_TRACE( T32, retptr0, T32, retptr1, TEND );

  return 0;
}

int32_t environ_get( int32_t environ, int32_t environ_buf )
{
  FUNC_TRACE( T32, environ, T32, environ_buf, TEND );

  return 0;
}

int32_t path_open( int32_t fd,
                   int32_t dirflags,
                   int32_t path,
                   int32_t path_len,
                   int32_t oflags,
                   int64_t fs_rights_base,
                   int64_t fs_rights_inheriting,
                   int32_t fdflags,
                   int32_t retptr0 )
{
  __wasi_fd_t retfd;

  FUNC_TRACE( T32,
              fd,
              T32,
              dirflags,
              T32,
              path,
              T32,
              path_len,
              T32,
              oflags,
              T64,
              fs_rights_base,
              T64,
              fs_rights_inheriting,
              T32,
              fdflags,
              T32,
              retptr0,
              TEND );

  // XXX TEMPORARY
  // attach_tree_ro_table_4( get_ro_table_3( 2 ) );
  // attach_tree_ro_table_4( get_ro_table_4( 0 ) );
  // fs = get_ro_table_4( 2 );
  // attach_blob_ro_mem_1( fs );

  for ( retfd = 4; retfd < N_FDS; retfd++ ) {
    if ( fds[retfd].open == false )
      break;
  }

  if ( find_file( path, path_len, fd, retfd ) != retfd ) {
    return __WASI_ERRNO_NOENT;
  } else {
    attach_blob_ro_mem_1( get_ro_table( retfd, 2 ) );
  }

  if ( retfd >= N_FDS )
    return __WASI_ERRNO_NFILE;

  fds[retfd].offset = 0;
  fds[retfd].size = size_ro_mem_1() - 1;
  fds[retfd].open = true;

  fds[retfd].stat.fs_filetype = __WASI_FILETYPE_REGULAR_FILE;
  fds[retfd].stat.fs_flags = __WASI_FDFLAGS_APPEND;
  fds[retfd].stat.fs_rights_base = __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_SEEK;
  fds[retfd].stat.fs_rights_inheriting = __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_SEEK;

  memory_copy_program( retptr0, &retfd, sizeof( retfd ) );
  RET_TRACE( retfd );
  return 0;
}

int32_t clock_res_get( int32_t id, int32_t retptr0 )
{
  FUNC_TRACE( T32, id, T32, retptr0, TEND );

  return 0;
}

int32_t clock_time_get( int32_t id, int64_t precision, int32_t retptr0 )
{
  FUNC_TRACE( T32, id, T64, precision, T32, retptr0, TEND );

  return 0;
}

int32_t poll_oneoff( int32_t in, int32_t out, int32_t nsubscriptions, int32_t retptr0 )
{
  FUNC_TRACE( T32, in, T32, out, T32, nsubscriptions, T32, retptr0, TEND );

  return 0;
}

int32_t sched_yield( void )
{
  FUNC_TRACE( TEND );

  return 0;
}

int32_t random_get( int32_t buf, int32_t buf_len )
{
  FUNC_TRACE( T32, buf, T32, buf_len, TEND );

  return 0;
}

int32_t sock_accept( int32_t fd, int32_t flags, int32_t retptr0 )
{
  FUNC_TRACE( T32, fd, T32, flags, T32, retptr0, TEND );

  return 0;
}

externref fixpoint_apply( externref encode )
{
  set_rw_table_0( 0, create_blob_i32( 0 ) );

  attach_tree_ro_table_0( encode );

  // Attach working directory to table 3 if exists
  if ( size_ro_table_0() >= 4 ) {
    attach_tree_ro_table_3( get_ro_table_0( 3 ) );
  }
  run_start();

  set_rw_table_0( 1, create_blob_rw_mem_1( fds[STDOUT].offset ) );

  set_rw_table_0( 2, create_blob_rw_mem_2( fds[STDERR].offset ) );

  return create_tree_rw_table_0( 3 );
}

externref get_ro_table( int32_t table_index, int32_t index )
{
  return get_ro_table_functions[table_index]( index );
}

void attach_tree_ro_table( int32_t table_index, externref handle )
{
  attach_tree_ro_table_functions[table_index]( handle );
}
