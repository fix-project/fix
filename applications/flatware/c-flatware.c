#include "c-flatware.h"
#include "filesys.h"
#include "flatware-decs.h"
#include "util/linked_list.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"

#define DO_TRACE 1
#define min( a, b ) ( ( a ) < ( b ) ? ( a ) : ( b ) )

typedef char __attribute__( ( address_space( 10 ) ) ) * externref;

enum fd_
{
  STDIN,
  STDOUT,
  STDERR,
};

static entry* filesys = NULL;
static fd_table fds;
static int64_t random_seed;
static uint64_t clock_time;

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

static void write_trace( const char* str, int32_t len )
{
  flatware_mem_unsafe_io( str, len );
}

static void write_uint( uint64_t val )
{
  static const char nums[] = "0123456789abcdef";
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
  write_trace( f_name, (int32_t)strlen( f_name ) );

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
{}

externref fixpoint_apply( externref encode ) __attribute( ( export_name( "_fixpoint_apply" ) ) );

/**
 * @brief Exits process, returning rval to the host environment.
 *
 * @param rval Return value
 */
_Noreturn void proc_exit( int32_t rval )
{
  FUNC_TRACE( T32, rval, TEND );

  set_rw_table( OutputRWTable, OUTPUT_EXIT_CODE, create_blob_i32( rval ) );
  flatware_exit();
}

/**
 * @brief Closes file descriptor.
 *
 * @param fd File descriptor
 * @return int32_t Status code
 */
int32_t fd_close( int32_t fd )
{
  FUNC_TRACE( T32, fd, TEND );

  return fd_table_remove( &fds, fd ) ? __WASI_ERRNO_SUCCESS : __WASI_ERRNO_BADF;
}

/**
 * @brief Gets file descriptor attributes.
 *
 * @param fd File descriptor
 * @param retptr0 Returns file descriptor attributes as __wasi_fdstat_t
 * @return int32_t Status code
 */
int32_t fd_fdstat_get( int32_t fd, int32_t retptr0 )
{
  FUNC_TRACE( T32, fd, T32, retptr0, TEND );

  filedesc* f = fd_table_find( &fds, fd );
  if ( f == NULL ) {
    return __WASI_ERRNO_BADF;
  }

  flatware_mem_to_program_mem( retptr0, (int32_t)&f->stat, sizeof( f->stat ) );

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Seeks to offset in file descriptor.
 *
 * @param fd File descriptor
 * @param offset Number of bytes to seek
 * @param whence Whence to seek from (SEEK_SET, SEEK_CUR, SEEK_END)
 * @param retptr0 Returns resulting offset as int64_t
 * @return int32_t Status code
 */
int32_t fd_seek( int32_t fd, int64_t offset, int32_t whence, int32_t retptr0 )
{
  FUNC_TRACE( T32, fd, T64, offset, T32, whence, T32, retptr0, TEND );

  filedesc* f = fd_table_find( &fds, fd );
  if ( f == NULL ) {
    return __WASI_ERRNO_BADF;
  }

  if ( !( f->stat.fs_rights_base & __WASI_RIGHTS_FD_SEEK ) ) {
    return __WASI_ERRNO_ACCES;
  }

  if ( f->stat.fs_filetype != __WASI_FILETYPE_REGULAR_FILE ) {
    return __WASI_ERRNO_INVAL;
  }

  switch ( whence ) {
    case __WASI_WHENCE_SET:
      f->offset = offset;
      break;
    case __WASI_WHENCE_CUR:
      f->offset += offset;
      break;
    case __WASI_WHENCE_END:
      f->offset = f->entry->stat.size + offset;
      break;
    default:
      return __WASI_ERRNO_INVAL;
  }

  flatware_mem_to_program_mem( retptr0, (int32_t)&f->offset, sizeof( f->offset ) );
  RET_TRACE( f->offset );

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Reads from file descriptor.
 *
 * @param fd File descriptor
 * @param iovs Array of __wasi_iovec_t structs containing buffers to read into
 * @param iovs_len Number of buffers in iovs
 * @param retptr0 Returns number of bytes read as int32_t
 * @return int32_t Status code
 */
int32_t fd_read( int32_t fd, int32_t iovs, int32_t iovs_len, int32_t retptr0 )
{
  FUNC_TRACE( T32, fd, T32, iovs, T32, iovs_len, T32, retptr0, TEND );

  filedesc* f = fd_table_find( &fds, fd );
  if ( f == NULL ) {
    return __WASI_ERRNO_BADF;
  }

  if ( !( f->stat.fs_rights_base & __WASI_RIGHTS_FD_READ ) ) {
    return __WASI_ERRNO_ACCES;
  }

  if ( f->stat.fs_filetype != __WASI_FILETYPE_REGULAR_FILE
       && f->stat.fs_filetype != __WASI_FILETYPE_CHARACTER_DEVICE ) {
    return __WASI_ERRNO_INVAL;
  }

  // Iterate over buffers
  int32_t bytes_read = 0;
  int32_t offset = f->offset;
  for ( int32_t i = 0; i < iovs_len; ++i ) {
    int32_t file_remaining = f->entry->stat.size - f->offset;
    if ( file_remaining <= 0 ) {
      break;
    }

    int32_t iobuf_offset
      = get_i32_program( iovs + i * (int32_t)sizeof( __wasi_iovec_t ) + (int32_t)offsetof( __wasi_iovec_t, buf ) );
    int32_t iobuf_len = get_i32_program( iovs + i * (int32_t)sizeof( __wasi_iovec_t )
                                         + (int32_t)offsetof( __wasi_iovec_t, buf_len ) );

    int32_t size_to_read = min( file_remaining, iobuf_len );
    flatware_mem_to_program_mem( iobuf_offset, (int32_t)f->entry->content + f->offset, size_to_read );
    offset += size_to_read;
    bytes_read += size_to_read;
  }
  f->offset = offset;

  flatware_mem_to_program_mem( retptr0, (int32_t)&bytes_read, sizeof( bytes_read ) );
  RET_TRACE( bytes_read );

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Writes to file descriptor.
 *
 * @param fd file descriptor
 * @param iovs Array of __wasi_iovec_t structs containing buffers to write from
 * @param iovs_len Number of buffers in iovs
 * @param retptr0 Returns number of bytes written as int32_t
 * @return int32_t Status code
 */
int32_t fd_write( int32_t fd, int32_t iovs, int32_t iovs_len, int32_t retptr0 )
{
  FUNC_TRACE( T32, fd, T32, iovs, T32, iovs_len, T32, retptr0, TEND );

  filedesc* f = fd_table_find( &fds, fd );
  if ( f == NULL ) {
    RAW_TRACE( "-> bad file descriptor", 23 );
    return __WASI_ERRNO_BADF;
  }

  if ( !( f->stat.fs_rights_base & __WASI_RIGHTS_FD_WRITE ) ) {
    RAW_TRACE( "-> no write rights", 15 );
    return __WASI_ERRNO_ACCES;
  }

  if ( f->stat.fs_filetype != __WASI_FILETYPE_REGULAR_FILE
       && f->stat.fs_filetype != __WASI_FILETYPE_CHARACTER_DEVICE ) {
    RAW_TRACE( "-> not a regular file", 21 );
    return __WASI_ERRNO_INVAL;
  }

  // Iterate over buffers
  int32_t bytes_written = 0;
  int32_t offset = f->stat.fs_flags & __WASI_FDFLAGS_APPEND ? f->entry->stat.size : f->offset;
  for ( int32_t i = 0; i < iovs_len; ++i ) {
    int32_t iobuf_offset = get_i32_program( iovs + i * (int32_t)sizeof( __wasi_ciovec_t )
                                            + (int32_t)offsetof( __wasi_ciovec_t, buf ) );
    int32_t iobuf_len = get_i32_program( iovs + i * (int32_t)sizeof( __wasi_ciovec_t )
                                         + (int32_t)offsetof( __wasi_ciovec_t, buf_len ) );

    if ( f->entry->stat.size < f->offset + iobuf_len && !entry_grow( f->entry, f->offset + iobuf_len ) ) {
      RAW_TRACE( "-> no space", 11 );
      return __WASI_ERRNO_NOSPC;
    }

    program_mem_to_flatware_mem( (int32_t)f->entry->content + f->offset, iobuf_offset, iobuf_len );

    if ( fd == STDOUT || fd == STDERR ) {
      flatware_mem_unsafe_io( (char*)f->entry->content + f->offset, iobuf_len );
    }

    offset += iobuf_len;
    bytes_written += iobuf_len;
  }
  if ( !( f->stat.fs_flags & __WASI_FDFLAGS_APPEND ) ) {
    f->offset = offset;
  }

  flatware_mem_to_program_mem( retptr0, (int32_t)&bytes_written, sizeof( bytes_written ) );
  RET_TRACE( bytes_written );

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Sets file descriptor flags.
 *
 * @param fd File descriptor
 * @param fdflags Flags to set as __wasi_fdflags_t
 * @return int32_t Status code
 */
int32_t fd_fdstat_set_flags( int32_t fd, int32_t fdflags )
{
  FUNC_TRACE( T32, fd, T32, fdflags, TEND );

  filedesc* f = fd_table_find( &fds, fd );
  if ( f == NULL ) {
    return __WASI_ERRNO_BADF;
  }

  if ( !( f->stat.fs_rights_base & __WASI_RIGHTS_FD_FDSTAT_SET_FLAGS ) ) {
    return __WASI_ERRNO_ACCES;
  }

  f->stat.fs_flags = fdflags;

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Gets file descriptor information about preopened files.
 *
 * @param fd File descriptor
 * @param retptr0 Returns file descriptor information as __wasi_prestat_t
 * @return int32_t Status code
 */
int32_t fd_prestat_get( int32_t fd, int32_t retptr0 )
{
  FUNC_TRACE( T32, fd, T32, retptr0, TEND );

  filedesc* f = fd_table_find( &fds, fd );
  if ( f == NULL ) {
    return __WASI_ERRNO_BADF;
  }

  if ( f->stat.fs_filetype != __WASI_FILETYPE_DIRECTORY ) {
    return __WASI_ERRNO_NOTDIR;
  }

  if ( !( f->stat.fs_rights_base & __WASI_RIGHTS_FD_FILESTAT_GET ) ) {
    return __WASI_ERRNO_ACCES;
  }

  __wasi_prestat_t prestat = { .tag = __WASI_PREOPENTYPE_DIR, .u.dir.pr_name_len = strlen( f->entry->name ) };
  flatware_mem_to_program_mem( retptr0, (int32_t)&prestat, sizeof( prestat ) );

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Gets path of preopened files.
 *
 * @param fd File descriptor
 * @param path String buffer to store path
 * @param path_len Length of path buffer
 * @return int32_t Status code
 */
int32_t fd_prestat_dir_name( int32_t fd, int32_t path, int32_t path_len )
{
  FUNC_TRACE( T32, fd, T32, path, T32, path_len, TEND );

  filedesc* f = fd_table_find( &fds, fd );
  if ( f == NULL ) {
    return __WASI_ERRNO_BADF;
  }

  if ( f->entry->stat.filetype != __WASI_FILETYPE_DIRECTORY ) {
    return __WASI_ERRNO_NOTDIR;
  }

  if ( !( f->stat.fs_rights_base & __WASI_RIGHTS_FD_FILESTAT_GET ) ) {
    return __WASI_ERRNO_ACCES;
  }

  flatware_mem_to_program_mem( path, (int32_t)f->entry->name, min( strlen( f->entry->name ), (uint64_t)path_len ) );

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Provides advisory information about a file descriptor.
 * @details No effect currently. Returns SUCCESS.
 *
 * @param fd File descriptor
 * @param offset Offset to advise
 * @param len Length of advice
 * @param advice Advice to give as __wasi_advice_t
 * @return int32_t Status code
 */
int32_t fd_advise( int32_t fd, int64_t offset, int64_t len, int32_t advice )
{
  FUNC_TRACE( T32, fd, T64, offset, T64, len, T32, advice, TEND );

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Allocates extra space for a file descriptor.
 *
 * @param fd File descriptor
 * @param offset Offset to allocate
 * @param len Length of allocation
 * @return int32_t Status code
 */
int32_t fd_allocate( int32_t fd, int64_t offset, int64_t len )
{
  FUNC_TRACE( T32, fd, T64, offset, T64, len, TEND );

  filedesc* f = fd_table_find( &fds, fd );
  if ( f == NULL ) {
    return __WASI_ERRNO_BADF;
  }

  if ( !( f->stat.fs_rights_base & __WASI_RIGHTS_FD_ALLOCATE ) ) {
    return __WASI_ERRNO_ACCES;
  }

  if ( f->stat.fs_filetype != __WASI_FILETYPE_REGULAR_FILE ) {
    return __WASI_ERRNO_INVAL;
  }

  if ( offset + len < 0 ) {
    return __WASI_ERRNO_INVAL;
  }

  if ( f->entry->stat.size < (uint64_t)( offset + len ) && !entry_grow( f->entry, offset + len ) ) {
    return __WASI_ERRNO_NOSPC;
  }

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Synchronizes the data of a file descriptor.
 * @details No effect currently. Returns SUCCESS.
 *
 * @param fd File descriptor
 * @return int32_t Status code
 */
int32_t fd_datasync( int32_t fd )
{
  FUNC_TRACE( T32, fd, TEND );

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Gets the attributes of a file descriptor.
 *
 * @param fd File descriptor
 * @param retptr0 Returns file descriptor attributes as __wasi_filestat_t
 * @return int32_t Status code
 */
int32_t fd_filestat_get( int32_t fd, int32_t retptr0 )
{
  FUNC_TRACE( T32, fd, T32, retptr0, TEND );

  filedesc* f = fd_table_find( &fds, fd );
  if ( f == NULL ) {
    return __WASI_ERRNO_BADF;
  }

  if ( !( f->stat.fs_rights_base & __WASI_RIGHTS_FD_FILESTAT_GET ) ) {
    return __WASI_ERRNO_ACCES;
  }

  flatware_mem_to_program_mem( retptr0, (int32_t)&f->entry->stat, sizeof( f->entry->stat ) );

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Adjusts the size of a file descriptor.
 *
 * @param fd File descriptor
 * @param size Size to set
 * @return int32_t Status code
 */
int32_t fd_filestat_set_size( int32_t fd, int64_t size )
{
  FUNC_TRACE( T32, fd, T64, size, TEND );

  if ( size < 0 ) {
    return __WASI_ERRNO_INVAL;
  }

  filedesc* f = fd_table_find( &fds, fd );
  if ( f == NULL ) {
    return __WASI_ERRNO_BADF;
  }

  if ( !( f->stat.fs_rights_base & __WASI_RIGHTS_FD_FILESTAT_SET_SIZE ) ) {
    return __WASI_ERRNO_ACCES;
  }

  if ( f->stat.fs_filetype != __WASI_FILETYPE_REGULAR_FILE ) {
    return __WASI_ERRNO_INVAL;
  }

  if ( size < 0 ) {
    return __WASI_ERRNO_INVAL;
  }

  if ( (uint64_t)size < f->entry->stat.size ) {
    f->entry->stat.size = size;
  } else if ( !entry_grow( f->entry, size ) ) {
    return __WASI_ERRNO_NOSPC;
  }

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Updates the timestamp metadata of a file descriptor.
 *
 * @param fd File descriptor
 * @param atim Accessed timestamp
 * @param mtim Modified timestamp
 * @param fst_flags Bitmask of __wasi_fstflags_t
 * @return int32_t
 */
int32_t fd_filestat_set_times( int32_t fd, int64_t atim, int64_t mtim, int32_t fst_flags )
{
  FUNC_TRACE( T32, fd, T64, atim, T64, mtim, T32, fst_flags, TEND );

  filedesc* f = fd_table_find( &fds, fd );
  if ( f == NULL ) {
    return __WASI_ERRNO_BADF;
  }

  if ( !( f->stat.fs_rights_base & __WASI_RIGHTS_FD_FILESTAT_SET_TIMES ) ) {
    return __WASI_ERRNO_ACCES;
  }

  if ( fst_flags & __WASI_FSTFLAGS_ATIM ) {
    f->entry->stat.atim = atim;
  }
  if ( fst_flags & __WASI_FSTFLAGS_ATIM_NOW ) {
    f->entry->stat.atim = clock_time;
  }
  if ( fst_flags & __WASI_FSTFLAGS_MTIM ) {
    f->entry->stat.mtim = mtim;
  }
  if ( fst_flags & __WASI_FSTFLAGS_MTIM_NOW ) {
    f->entry->stat.mtim = clock_time;
  }

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Reads from a file descriptor at a given offset without changing the file descriptor's offset.
 *
 * @param fd File descriptor
 * @param iovs Array of __wasi_iovec_t structs containing buffers to read from
 * @param iovs_len Number of buffers in iovs
 * @param offset Offset to read from
 * @param retptr0 Number of bytes read
 * @return int32_t Status code
 */
int32_t fd_pread( int32_t fd, int32_t iovs, int32_t iovs_len, int64_t offset, int32_t retptr0 )
{
  FUNC_TRACE( T32, fd, T32, iovs, T32, iovs_len, T64, offset, T32, retptr0, TEND );

  filedesc* f = fd_table_find( &fds, fd );
  if ( f == NULL ) {
    return __WASI_ERRNO_BADF;
  }

  if ( !( f->stat.fs_rights_base & __WASI_RIGHTS_FD_READ ) ) {
    return __WASI_ERRNO_ACCES;
  }

  if ( f->stat.fs_filetype != __WASI_FILETYPE_REGULAR_FILE
       && f->stat.fs_filetype != __WASI_FILETYPE_CHARACTER_DEVICE ) {
    return __WASI_ERRNO_INVAL;
  }

  // Iterate over buffers
  int32_t bytes_read = 0;
  for ( int32_t i = 0; i < iovs_len; ++i ) {
    int32_t file_remaining = f->entry->stat.size - f->offset;
    if ( file_remaining <= 0 ) {
      break;
    }

    int32_t iobuf_offset
      = get_i32_program( iovs + i * (int32_t)sizeof( __wasi_iovec_t ) + (int32_t)offsetof( __wasi_iovec_t, buf ) );
    int32_t iobuf_len = get_i32_program( iovs + i * (int32_t)sizeof( __wasi_iovec_t )
                                         + (int32_t)offsetof( __wasi_iovec_t, buf_len ) );

    int32_t size_to_read = min( file_remaining, iobuf_len );
    flatware_mem_to_program_mem( iobuf_offset, (int32_t)f->entry->content + f->offset, size_to_read );
    offset += size_to_read;
    bytes_read += size_to_read;
  }

  flatware_mem_to_program_mem( retptr0, (int32_t)&bytes_read, sizeof( bytes_read ) );
  RET_TRACE( bytes_read );

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Writes to a file descriptor at a given offset without changing the file descriptor's offset.
 *
 * @param fd File descriptor
 * @param iovs Array of __wasi_ciovec_t structs containing buffers to write from
 * @param iovs_len Number of buffers in iovs
 * @param offset Offset to write to
 * @param retptr0 Number of bytes written
 * @return int32_t Status code
 */
int32_t fd_pwrite( int32_t fd, int32_t iovs, int32_t iovs_len, int64_t offset, int32_t retptr0 )
{
  FUNC_TRACE( T32, fd, T32, iovs, T32, iovs_len, T64, offset, T32, retptr0, TEND );

  filedesc* f = fd_table_find( &fds, fd );
  if ( f == NULL ) {
    return __WASI_ERRNO_BADF;
  }

  if ( !( f->stat.fs_rights_base & __WASI_RIGHTS_FD_WRITE ) ) {
    return __WASI_ERRNO_ACCES;
  }

  if ( f->stat.fs_filetype != __WASI_FILETYPE_REGULAR_FILE
       && f->stat.fs_filetype != __WASI_FILETYPE_CHARACTER_DEVICE ) {
    return __WASI_ERRNO_INVAL;
  }

  // Iterate over buffers
  int32_t bytes_written = 0;
  if ( f->stat.fs_flags & __WASI_FDFLAGS_APPEND ) {
    offset = f->entry->stat.size;
  }
  for ( int32_t i = 0; i < iovs_len; ++i ) {
    int32_t iobuf_offset = get_i32_program( iovs + i * (int32_t)sizeof( __wasi_ciovec_t )
                                            + (int32_t)offsetof( __wasi_ciovec_t, buf ) );
    int32_t iobuf_len = get_i32_program( iovs + i * (int32_t)sizeof( __wasi_ciovec_t )
                                         + (int32_t)offsetof( __wasi_ciovec_t, buf_len ) );

    if ( f->entry->stat.size < f->offset + iobuf_len && !entry_grow( f->entry, f->offset + iobuf_len ) ) {
      return __WASI_ERRNO_NOSPC;
    }

    program_mem_to_flatware_mem( (int32_t)f->entry->content + f->offset, iobuf_offset, iobuf_len );

    if ( fd == STDOUT ) {
      flatware_mem_unsafe_io( (char*)f->entry->content + f->offset, iobuf_len );
    }

    offset += iobuf_len;
    bytes_written += iobuf_len;
  }

  flatware_mem_to_program_mem( retptr0, (int32_t)&bytes_written, sizeof( bytes_written ) );
  RET_TRACE( bytes_written );

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Reads directory entries.
 *
 * @param fd File descriptor
 * @param buf Buffer to store directory entries
 * @param buf_len Buffer length
 * @param cookie Offset to start reading from
 * @param retptr0 Number of bytes read
 * @return int32_t Status code
 */
int32_t fd_readdir( int32_t fd, int32_t buf, int32_t buf_len, int64_t cookie, int32_t retptr0 )
{
  FUNC_TRACE( T32, fd, T32, buf, T32, buf_len, T64, cookie, T32, retptr0, TEND );

  filedesc* f = fd_table_find( &fds, fd );
  if ( f == NULL ) {
    return __WASI_ERRNO_BADF;
  }

  if ( f->stat.fs_filetype != __WASI_FILETYPE_DIRECTORY ) {
    return __WASI_ERRNO_NOTDIR;
  }

  if ( !( f->stat.fs_rights_base & __WASI_RIGHTS_FD_READDIR ) ) {
    return __WASI_ERRNO_ACCES;
  }

  if ( cookie < 0 || (uint64_t)cookie >= f->entry->stat.size ) {
    return __WASI_ERRNO_INVAL;
  }

  int32_t bytes_read = 0;
  struct list_elem* e = list_begin( &f->entry->children );
  for ( int64_t i = 0; i < cookie; ++i ) {
    e = list_next( e );
    if ( e == list_end( &f->entry->children ) ) {
      break;
    }
  }

  while ( e != list_end( &f->entry->children ) ) {
    entry* child = list_entry( e, entry, elem );
    __wasi_dirent_t dirent = { .d_next = list_next( e ) == list_end( &f->entry->children ) ? 0 : cookie++,
                               .d_ino = child->stat.ino,
                               .d_type = child->stat.filetype,
                               .d_namlen = strlen( child->name ) };

    int32_t child_size = sizeof( dirent ) + dirent.d_namlen;
    if ( bytes_read + child_size > buf_len ) {
      break;
    }

    flatware_mem_to_program_mem( buf + bytes_read, (int32_t)&dirent, sizeof( dirent ) );
    flatware_mem_to_program_mem( buf + bytes_read + sizeof( dirent ), (int32_t)child->name, dirent.d_namlen );
    bytes_read += child_size;
    e = list_next( e );
  }

  flatware_mem_to_program_mem( retptr0, (int32_t)&bytes_read, sizeof( bytes_read ) );

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Synchronizes file and metadata changes to storage.
 * @details No effect currently. Returns SUCCESS.
 *
 * @param fd File descriptor
 * @return int32_t Status code
 */
int32_t fd_sync( int32_t fd )
{
  FUNC_TRACE( T32, fd, TEND );

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Gets the current offset of a file descriptor.
 *
 * @param fd File descriptor
 * @param retptr0 Returns offset as int32_t
 * @return int32_t Status code
 */
int32_t fd_tell( int32_t fd, int32_t retptr0 )
{
  FUNC_TRACE( T32, fd, T32, retptr0, TEND );

  filedesc* f = fd_table_find( &fds, fd );
  if ( f == NULL ) {
    return __WASI_ERRNO_BADF;
  }

  if ( !( f->stat.fs_rights_base & __WASI_RIGHTS_FD_TELL ) ) {
    return __WASI_ERRNO_ACCES;
  }

  flatware_mem_to_program_mem( retptr0, (int32_t)&f->offset, sizeof( f->offset ) );
  RET_TRACE( f->offset );

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Creates a directory.
 *
 * @param fd Base directory for path
 * @param path Path of directory
 * @param path_len Path length
 * @return int32_t Status code
 */
int32_t path_create_directory( int32_t fd, int32_t path, int32_t path_len )
{
  FUNC_TRACE( T32, fd, T32, path, T32, path_len, TEND );

  filedesc* f = fd_table_find( &fds, fd );
  if ( f == NULL ) {
    return __WASI_ERRNO_BADF;
  }

  if ( !( f->stat.fs_rights_base & __WASI_RIGHTS_PATH_CREATE_DIRECTORY ) ) {
    return __WASI_ERRNO_ACCES;
  }

  if ( f->stat.fs_filetype != __WASI_FILETYPE_DIRECTORY ) {
    return __WASI_ERRNO_NOTDIR;
  }

  char* name = (char*)malloc( path_len + 1 );
  if ( name == NULL ) {
    return __WASI_ERRNO_NOMEM;
  }

  program_mem_to_flatware_mem( (int32_t)name, path, path_len );
  name[path_len] = '\0';

  char *saveptr = name, *token = strtok_r( name, "/", &saveptr );
  char* next_token = strtok_r( NULL, "/", &saveptr );
  while ( next_token != NULL ) {
    entry* child = entry_find( f->entry, token );
    if ( child == NULL ) {
      return __WASI_ERRNO_NOENT;
    }

    if ( child->stat.filetype != __WASI_FILETYPE_DIRECTORY ) {
      return __WASI_ERRNO_NOTDIR;
    }

    token = next_token;
    next_token = strtok_r( NULL, "/", &saveptr );
  }

  entry* child = entry_find( f->entry, token );
  if ( child != NULL ) {
    return __WASI_ERRNO_EXIST;
  }

  entry* new_entry = entry_create( token, __WASI_FILETYPE_DIRECTORY );
  free( name );
  if ( new_entry == NULL ) {
    return __WASI_ERRNO_NOMEM;
  }

  list_push_back( &f->entry->children, &new_entry->elem );
  f->entry->stat.size++;

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Gets file metadata
 *
 * @param fd Base directory for path
 * @param flags Path flags as __wasi_lookupflags_t
 * @param path Path to file
 * @param path_len Path length
 * @param retptr0 Returns file metadata as __wasi_filestat_t
 * @return int32_t Status code
 */
int32_t path_filestat_get( int32_t fd, int32_t flags, int32_t path, int32_t path_len, int32_t retptr0 )
{
  FUNC_TRACE( T32, fd, T32, flags, T32, path, T32, path_len, T32, retptr0, TEND );

  filedesc* f = fd_table_find( &fds, fd );
  if ( f == NULL ) {
    return __WASI_ERRNO_BADF;
  }

  if ( !( f->stat.fs_rights_base & __WASI_RIGHTS_PATH_FILESTAT_GET ) ) {
    return __WASI_ERRNO_ACCES;
  }

  if ( f->stat.fs_filetype != __WASI_FILETYPE_DIRECTORY ) {
    return __WASI_ERRNO_NOTDIR;
  }

  char* name = (char*)malloc( path_len + 1 );
  if ( name == NULL ) {
    return __WASI_ERRNO_NOMEM;
  }

  program_mem_to_flatware_mem( (int32_t)name, path, path_len );
  name[path_len] = '\0';

  entry* e = f->entry;
  char *saveptr = name, *token;
  while ( ( token = strtok_r( saveptr, "/", &saveptr ) ) != NULL ) {
    e = entry_find( e, token );
    if ( e == NULL ) {
      break;
    }
  }
  free( name );

  if ( e == NULL ) {
    return __WASI_ERRNO_NOENT;
  }

  flatware_mem_to_program_mem( retptr0, (int32_t)&e->stat, sizeof( e->stat ) );

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Sets the time metadata for a file.
 *
 * @param fd Base directory for path
 * @param flags Path flags as __wasi_lookupflags_t
 * @param path Path to file
 * @param path_len Length of path
 * @param atim Last accessed time
 * @param mtim Last modified time
 * @param fst_flags Bitmask of __wasi_fstflags_t
 * @return int32_t Status code
 */
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

/**
 * @brief Creates a hard link.
 *
 * @param old_fd Base directory for source path
 * @param old_flags Source flags
 * @param old_path Source path
 * @param old_path_len Source path length
 * @param new_fd Base directory for destination path
 * @param new_path Destination path
 * @param new_path_len Destination path length
 * @return int32_t Status code
 */
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

/**
 * @brief Reads the target path of a symlink
 *
 * @param fd Base directory for path
 * @param path Path
 * @param path_len Path length
 * @param buf Target path buffer
 * @param buf_len Buffer length
 * @param retptr0 Number of bytes read
 * @return int32_t Status code
 */
int32_t path_readlink( int32_t fd, int32_t path, int32_t path_len, int32_t buf, int32_t buf_len, int32_t retptr0 )
{
  FUNC_TRACE( T32, fd, T32, path, T32, path_len, T32, buf, T32, buf_len, T32, retptr0, TEND );

  return __WASI_ERRNO_NOLINK;
}

/**
 * @brief Removes a directory.
 *
 * @param fd Base directory for path
 * @param path Path to directory
 * @param path_len Path length
 * @return int32_t Status code
 */
int32_t path_remove_directory( int32_t fd, int32_t path, int32_t path_len )
{
  FUNC_TRACE( T32, fd, T32, path, T32, path_len, TEND );

  return 0;
}

/**
 * @brief Renames a file.
 *
 * @param fd Base directory for source path
 * @param old_path Source path
 * @param old_path_len Source path length
 * @param new_fd Base directory for destination path
 * @param new_path Destination path
 * @param new_path_len Destination path length
 * @return int32_t Status code
 */
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

/**
 * @brief Creates a symbolic link.
 *
 * @param old_path Source path
 * @param old_path_len Source path length
 * @param fd Base directory for paths
 * @param new_path Destination path
 * @param new_path_len Destination path length
 * @return int32_t Status code
 */
int32_t path_symlink( int32_t old_path, int32_t old_path_len, int32_t fd, int32_t new_path, int32_t new_path_len )
{
  FUNC_TRACE( T32, old_path, T32, old_path_len, T32, fd, T32, new_path, T32, new_path_len, TEND );

  return 0;
}

/**
 * @brief Unlinks a file at the given path.
 *
 * @param fd Base directory for path
 * @param path Path to file
 * @param path_len Length of path
 * @return int32_t Status code
 */
int32_t path_unlink_file( int32_t fd, int32_t path, int32_t path_len )
{
  FUNC_TRACE( T32, fd, T32, path, T32, path_len, TEND );

  return 0;
}

/**
 * @brief Gets the number of the arguments.
 *
 * @param num_argument_ptr Number of arguments
 * @param size_argument_ptr Size of arguments in bytes
 * @return int32_t Status code
 */
int32_t args_sizes_get( int32_t num_argument_ptr, int32_t size_argument_ptr )
{
  int32_t num;
  int32_t size = 0;

  FUNC_TRACE( T32, num_argument_ptr, T32, size_argument_ptr, TEND );

  attach_tree_ro_table( ScratchROTable, get_ro_table( InputROTable, INPUT_ARGS ) );
  num = size_ro_table( ScratchROTable );

  flatware_mem_to_program_mem( num_argument_ptr, (int32_t)&num, 4 );
  RET_TRACE( num );

  // Actual arguments
  for ( int32_t i = 0; i < num; i++ ) {
    attach_blob_ro_mem( ScratchROMem, get_ro_table( ScratchROTable, i ) );
    size += byte_size_ro_mem( ScratchROMem );
  }

  flatware_mem_to_program_mem( size_argument_ptr, (int32_t)&size, 4 );
  RAW_TRACE( ", ", 2 );
  INT_TRACE( size );
  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Gets the arguments.
 *
 * @param argv_ptr Argument array pointer (char**)
 * @param argv_buf_ptr Argument buffer pointer (char*)
 * @return int32_t Status code
 */
int32_t args_get( int32_t argv_ptr, int32_t argv_buf_ptr )
{
  FUNC_TRACE( T32, argv_ptr, T32, argv_buf_ptr, TEND );

  int32_t size = 0;
  int32_t addr = argv_buf_ptr;

  attach_tree_ro_table( ScratchROTable, get_ro_table( InputROTable, INPUT_ARGS ) );
  for ( int32_t i = 0; i < size_ro_table( ScratchROTable ); i++ ) {
    attach_blob_ro_mem( ScratchROMem, get_ro_table( ScratchROTable, i ) );
    size = byte_size_ro_mem( ScratchROMem );
    flatware_mem_to_program_mem( argv_ptr + i * (int32_t)sizeof( char* ), (int32_t)&addr, sizeof( char* ) );
    ro_mem_to_program_mem( ScratchROMem, addr, 0, size );
    addr += size;
  }

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Gets the environment variables.
 *
 * @param retptr0 Returns number of environment variables as int32_t
 * @param retptr1 Returns size of environment variables in bytes as int32_t
 * @return int32_t Status code
 */
int32_t environ_sizes_get( int32_t retptr0, int32_t retptr1 )
{
  attach_tree_ro_table( ScratchROTable, get_ro_table( InputROTable, INPUT_ENV ) );

  int32_t size = 0;
  int32_t num = size_ro_table( ScratchROTable );

  FUNC_TRACE( T32, retptr0, T32, retptr1, TEND );

  for ( int32_t i = 0; i < num; i++ ) {
    attach_blob_ro_mem( ScratchROMem, get_ro_table( ScratchROTable, i ) );
    size += byte_size_ro_mem( ScratchROMem );
  }

  flatware_mem_to_program_mem( retptr0, (int32_t)&num, sizeof( num ) );
  flatware_mem_to_program_mem( retptr1, (int32_t)&size, sizeof( size ) );

  RET_TRACE( num );
  RAW_TRACE( ", ", 2 );
  INT_TRACE( size );

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Gets the environment variables.
 *
 * @param environ Environment variable array pointer (char**)
 * @param environ_buf Environment variable buffer pointer (char*)
 * @return int32_t Status code
 */
int32_t environ_get( int32_t environ, int32_t environ_buf )
{
  FUNC_TRACE( T32, environ, T32, environ_buf, TEND );

  int32_t size = 0;
  int32_t addr = environ_buf;

  attach_tree_ro_table( ScratchROTable, get_ro_table( InputROTable, INPUT_ENV ) );

  for ( int32_t i = 0; i < size_ro_table( ScratchROTable ); i++ ) {
    attach_blob_ro_mem( ScratchROMem, get_ro_table( ScratchROTable, i ) );
    size = byte_size_ro_mem( ScratchROMem );
    flatware_mem_to_program_mem( environ + i * (int32_t)sizeof( char* ), (int32_t)&addr, sizeof( char* ) );
    ro_mem_to_program_mem( ScratchROMem, addr, 0, size );
    addr += size;
  }

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Opens file descriptor.
 *
 * @param fd Base directory for path
 * @param dirflags Directory flags
 * @param path Path to file
 * @param path_len Length of path
 * @param oflags Open flags
 * @param fs_rights_base Base file rights
 * @param fs_rights_inheriting Inherited file rights
 * @param fdflags File descriptor flags
 * @param retptr0 Returns file descriptor as __wasi_fd_t
 * @return int32_t Status code
 */
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

  filedesc* f = fd_table_find( &fds, fd );
  if ( f == NULL ) {
    return __WASI_ERRNO_BADF;
  }

  char* path_str = (char*)malloc( path_len + 1 );
  if ( path_str == NULL ) {
    return __WASI_ERRNO_NOMEM;
  }
  program_mem_to_flatware_mem( (int32_t)path_str, path, path_len );
  path_str[path_len] = '\0';

  char *token, *saveptr = path_str;
  entry* e = f->entry;
  while ( e != NULL && ( token = strtok_r( saveptr, "/", &saveptr ) ) != NULL ) {
    if ( strlen( token ) == 0 || strcmp( token, "." ) == 0 ) {
      continue;
    }

    if ( strcmp( token, ".." ) == 0 ) {
      e = e->stat.filetype == __WASI_FILETYPE_DIRECTORY ? e->parent : NULL;
    }

    e = entry_find( e, token );
  }
  free( path_str );

  if ( e == NULL ) {
    return __WASI_ERRNO_NOENT;
  }

  filedesc* new_fd = fd_table_insert( &fds, e );
  flatware_mem_to_program_mem( retptr0, (int32_t)&new_fd->index, sizeof( new_fd->index ) );
  RET_TRACE( new_fd->index );

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Gets the resolution of a clock.
 * @details No effect currently. Returns INVAL.
 *
 * @param id Clock ID
 * @param retptr0 Returns resolution as int64_t
 * @return int32_t Status code
 */
int32_t clock_res_get( int32_t id, int32_t retptr0 )
{
  FUNC_TRACE( T32, id, T32, retptr0, TEND );

  return __WASI_ERRNO_INVAL;
}

/**
 * @brief Gets the time of a clock.
 *
 * @param id Clock ID
 * @param precision Maximum permitted error in nanoseconds
 * @param retptr0 Returns time as int64_t
 * @return int32_t Status code
 */
int32_t clock_time_get( int32_t id, int64_t precision, int32_t retptr0 )
{
  FUNC_TRACE( T32, id, T64, precision, T32, retptr0, TEND );

  return __WASI_ERRNO_INVAL;
}

/**
 * @brief Concurrently polls for a set of events
 *
 * @param in The events to subscribe to (array of __wasi_subscription_t)
 * @param out The events that have occurred (array of __wasi_event_t)
 * @param nsubscriptions Number of subscriptions
 * @param retptr0 Returns number of events as int32_t
 * @return int32_t Status code
 */
int32_t poll_oneoff( int32_t in, int32_t out, int32_t nsubscriptions, int32_t retptr0 )
{
  FUNC_TRACE( T32, in, T32, out, T32, nsubscriptions, T32, retptr0, TEND );

  return 0;
}

/**
 * @brief Yields the execution of the current process.
 *
 * @return int32_t Status code
 */
int32_t sched_yield( void )
{
  FUNC_TRACE( TEND );

  return 0;
}

/**
 * @brief Gets a random value.
 *
 * @param buf Result buffer
 * @param buf_len Buffer size
 * @return int32_t
 */
int32_t random_get( int32_t buf, int32_t buf_len )
{
  uint8_t random;
  FUNC_TRACE( T32, buf, T32, buf_len, TEND );

  for ( int32_t i = 0; i < buf_len; i++ ) {
    int64_t a = 16807;
    int64_t m = 2147483647;
    random_seed = ( a * random_seed ) % m;

    random = (uint8_t)( random_seed % 256 );
    flatware_mem_to_program_mem( buf + i, (int32_t)&random, 1 );
  }

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Accepts a new connection on a socket.
 *
 * @param fd File descriptor
 * @param flags Flags
 * @param retptr0 New file descriptor
 * @return int32_t Status code
 */
int32_t sock_accept( int32_t fd, int32_t flags, int32_t retptr0 )
{
  FUNC_TRACE( T32, fd, T32, flags, T32, retptr0, TEND );

  return 0;
}

/**
 * @brief Receives a message from a socket.
 *
 * @param fd File descriptor of socket
 * @param ri_data Array of __wasi_iovec_t structs containing buffers to read into
 * @param ri_data_len Number of buffers in ri_data
 * @param ri_flags Flags
 * @param retptr0 Number of bytes read
 * @param retptr1 Message flags
 * @return int32_t Status code
 */
int32_t sock_recv( int32_t fd,
                   int32_t ri_data,
                   int32_t ri_data_len,
                   int32_t ri_flags,
                   int32_t retptr0,
                   int32_t retptr1 )
{
  FUNC_TRACE( T32, fd, T32, ri_data, T32, ri_data_len, T32, ri_flags, T32, retptr0, T32, retptr1, TEND );

  return 0;
}

/**
 * @brief Sends a message on a socket.
 *
 * @param fd File descriptor of socket
 * @param si_data Array of __wasi_ciovec_t structs containing buffers to send
 * @param si_data_len Number of buffers in si_data
 * @param si_flags Flags
 * @param retptr0 Number of bytes sent
 * @return int32_t Status code
 */
int32_t sock_send( int32_t fd, int32_t si_data, int32_t si_data_len, int32_t si_flags, int32_t retptr0 )
{
  FUNC_TRACE( T32, fd, T32, si_data, T32, si_data_len, T32, si_flags, T32, retptr0, TEND );

  return 0;
}

/**
 * @brief Closes a socket.
 *
 * @param fd File descriptor of socket
 * @param how How to close the socket as __wasi_sdflags_t
 * @return int32_t Status code
 */
int32_t sock_shutdown( int32_t fd, int32_t how )
{
  FUNC_TRACE( T32, fd, T32, how, TEND );

  return 0;
}

externref fixpoint_apply( externref encode )
{
  attach_tree_ro_table( InputROTable, encode );

  // Load filesystem
  filesys = entry_load_filesys( NULL, get_ro_table( InputROTable, INPUT_FILESYSTEM ) );
  fd_table_init( &fds );

  // Load standard I/O
  entry* stdin = entry_from_blob( "stdin", get_ro_table( InputROTable, INPUT_STDIN ) );
  entry* stdout = entry_create( "stdout", __WASI_FILETYPE_CHARACTER_DEVICE );
  entry* stderr = entry_create( "stderr", __WASI_FILETYPE_CHARACTER_DEVICE );

  // Attach standard I/O to file descriptors
  filedesc* stdin_fd = fd_table_insert_at( &fds, stdin, STDIN );
  filedesc* stdout_fd = fd_table_insert_at( &fds, stdout, STDOUT );
  filedesc* stderr_fd = fd_table_insert_at( &fds, stderr, STDERR );

  // Set standard I/O flags, rights, and types
  stdin_fd->stat.fs_rights_base = __WASI_RIGHTS_FD_READ;
  stdin_fd->stat.fs_rights_inheriting = 0;

  stdout_fd->stat.fs_rights_base = __WASI_RIGHTS_FD_WRITE;
  stdout_fd->stat.fs_rights_inheriting = 0;

  stderr_fd->stat.fs_rights_base = __WASI_RIGHTS_FD_WRITE;
  stderr_fd->stat.fs_rights_inheriting = 0;

  // Preload filesystem root
  fd_table_insert( &fds, filesys );

  // Load random seed
  attach_blob_ro_mem( ScratchROMem, get_ro_table( InputROTable, INPUT_RANDOM_SEED ) );
  random_seed = get_i32_ro_mem( ScratchROMem, 0 );

  // Load clock time
  clock_time = 0;

  // Prepare output
  grow_rw_table( OutputRWTable, 4, create_blob_i32( 0 ) );

  // Run program
  RAW_TRACE( "run_start", 10 );
  run_start();
  RAW_TRACE( "\nrun_finish\n", 12 );

  // Save filesystem
  fd_table_destroy( &fds );
  externref filesys_ref = entry_save_filesys( filesys );
  entry_free( filesys );
  set_rw_table( OutputRWTable, OUTPUT_FILESYSTEM, filesys_ref );

  // Save standard I/O
  set_rw_table( OutputRWTable, OUTPUT_STDOUT, entry_to_blob( stdout ) );
  set_rw_table( OutputRWTable, OUTPUT_STDERR, entry_to_blob( stderr ) );

  // Destroy standard I/O
  entry_free( stdin );
  entry_free( stdout );
  entry_free( stderr );

  return create_tree_rw_table( OutputRWTable, 4 );
}

#pragma clang diagnostic pop
