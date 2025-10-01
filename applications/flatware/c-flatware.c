#include "c-flatware.h"
#include "api.h"
#include "filesys.h"
#include "flatware-decs.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"

#define DO_TRACE 1
#define N_FDS 8
#define N_FILES 8

typedef char __attribute__( ( address_space( 10 ) ) ) * externref;

typedef struct filedesc
{
  unsigned long offset;
  int32_t size;
  __wasi_fdstat_t stat;
  int32_t file_id;
  bool open;
  char padding[3];
} filedesc;

typedef struct file
{
  bool in_use;
  bool is_dir;
  char padding[2];
  // if is_dir, -1, else the ro_mem holding the content
  int32_t mem_id;
  int32_t num_fds;
  struct substring name;
} file;

enum fd_
{
  STDIN,
  STDOUT,
  STDERR,
  WORKING_DIRECTORY,
  FILE_START,
};

static filedesc fds[N_FDS] = {
  { .open = true,
    .size = 0,
    .offset = 0,
    .file_id = 0,
    .stat = { .fs_filetype = __WASI_FILETYPE_CHARACTER_DEVICE,
              .fs_rights_base = __WASI_RIGHTS_FD_READ,
              .fs_rights_inheriting = __WASI_RIGHTS_FD_READ,
              .fs_flags = 0 } }, // STDIN
  { .open = true,
    .size = 0,
    .offset = 0,
    .file_id = 1,
    .stat = { .fs_filetype = __WASI_FILETYPE_CHARACTER_DEVICE,
              .fs_rights_base = __WASI_RIGHTS_FD_WRITE,
              .fs_rights_inheriting = __WASI_RIGHTS_FD_WRITE,
              .fs_flags = 0 } }, // STDOUT
  { .open = true,
    .size = 0,
    .offset = 0,
    .file_id = 2,
    .stat = { .fs_filetype = __WASI_FILETYPE_CHARACTER_DEVICE,
              .fs_rights_base = __WASI_RIGHTS_FD_WRITE,
              .fs_rights_inheriting = __WASI_RIGHTS_FD_WRITE,
              .fs_flags = 0 } }, // STDERR
  { .open = true,
    .size = -1,
    .offset = 0,
    .file_id = 3,
    .stat = { .fs_filetype = __WASI_FILETYPE_DIRECTORY,
              .fs_rights_base = __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_WRITE | __WASI_RIGHTS_FD_READDIR,
              .fs_rights_inheriting = __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_WRITE | __WASI_RIGHTS_FD_READDIR,
              .fs_flags = 0 } }, // WORKING DIRECTORY
};

static file files[N_FILES]
  = { { .in_use = true, .is_dir = false, .name = { "stdin", 5 }, .mem_id = StdInROMem, .num_fds = 1 },
      { .in_use = true, .is_dir = false, .name = { "stdout", 6 }, .mem_id = StdOutRWMem, .num_fds = 1 },
      { .in_use = true, .is_dir = false, .name = { "stderr", 6 }, .mem_id = StdErrRWMem, .num_fds = 1 },
      { .in_use = true, .is_dir = true, .name = { ".", 1 }, .mem_id = -1, .num_fds = 1 } };

static unsigned int* ro_mem_use;

static int64_t random_seed;

// Bitset functions
static bool bitset_get( unsigned int*, uint32_t );
static void bitset_set( unsigned int*, uint32_t );
static void bitset_clear( unsigned int*, uint32_t );

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

static int32_t trace_offset = 0;
static void write_trace( const char* str, int32_t len )
{
  if ( trace_offset + len > page_size_rw_mem( StdTraceRWMem ) * WASM_RT_PAGESIZE ) {
    grow_rw_mem_pages( StdTraceRWMem, len / WASM_RT_PAGESIZE + 1 );
  }
  flatware_mem_to_rw_mem( StdTraceRWMem, trace_offset, (int32_t)str, len );
  trace_offset += len;
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
  file* file;

  FUNC_TRACE( T32, fd, TEND );

  if ( fd >= N_FDS ) {
    return __WASI_ERRNO_MFILE;
  }

  if ( fd < FILE_START || fds[fd].open == false ) {
    return __WASI_ERRNO_BADF;
  }

  file = &files[fds[fd].file_id];
  file->num_fds--;
  fds[fd].open = false;

  if ( file->num_fds == 0 ) {
    file->in_use = false;
    if ( !file->is_dir ) {
      bitset_clear( ro_mem_use, (uint32_t)file->mem_id );
    }
  }

  return __WASI_ERRNO_SUCCESS;
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

  if ( fd >= N_FDS ) {
    return __WASI_ERRNO_MFILE;
  }

  if ( fds[fd].open == false ) {
    return __WASI_ERRNO_BADF;
  }

  // Copy stat struct to program memory
  flatware_mem_to_program_mem( retptr0, (int32_t)&fds[fd].stat, sizeof( fds[fd].stat ) );
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

  if ( fd >= N_FDS ) {
    return __WASI_ERRNO_MFILE;
  }

  if ( fd < FILE_START ) {
    return __WASI_ERRNO_BADF;
  }

  switch ( whence ) {
    case __WASI_WHENCE_SET:
      break;
    case __WASI_WHENCE_CUR:
      offset = fds[fd].offset + offset;
      break;
    case __WASI_WHENCE_END:
      offset = fds[fd].size + offset;
      break;
    default:
      return __WASI_ERRNO_INVAL;
  }

  fds[fd].offset = (unsigned long)offset;

  flatware_mem_to_program_mem( retptr0, (int32_t)&offset, sizeof( offset ) );
  RET_TRACE( offset );
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
  int32_t total_read = 0;
  int32_t iobuf_offset, iobuf_len;
  int32_t size_to_read;
  file f;

  FUNC_TRACE( T32, fd, T32, iovs, T32, iovs_len, T32, retptr0, TEND );

  if ( fd >= N_FDS ) {
    return __WASI_ERRNO_MFILE;
  }

  if ( ( fd < FILE_START && fd != STDIN ) || fds[fd].open == false ) {
    return __WASI_ERRNO_BADF;
  }

  f = files[fds[fd].file_id];

  if ( f.is_dir ) {
    return __WASI_ERRNO_BADF;
  }

  // Iterate over buffers
  for ( int32_t i = 0; i < iovs_len; ++i ) {
    int32_t file_remaining = fds[fd].size - (int32_t)fds[fd].offset;

    if ( file_remaining == 0 ) {
      break;
    }

    iobuf_offset
      = get_i32_program( iovs + i * (int32_t)sizeof( __wasi_iovec_t ) + (int32_t)offsetof( __wasi_iovec_t, buf ) );
    iobuf_len = get_i32_program( iovs + i * (int32_t)sizeof( __wasi_iovec_t )
                                 + (int32_t)offsetof( __wasi_iovec_t, buf_len ) );

    size_to_read = iobuf_len < file_remaining ? iobuf_len : file_remaining;

    ro_mem_to_program_mem( f.mem_id, iobuf_offset, (int32_t)fds[fd].offset, size_to_read );
    fds[fd].offset += (unsigned long)size_to_read;
    total_read += size_to_read;
  }

  flatware_mem_to_program_mem( retptr0, (int32_t)&total_read, sizeof( total_read ) );
  RET_TRACE( total_read );
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
  int32_t iobuf_offset, iobuf_len;
  int32_t total_written = 0;
  file f;

  FUNC_TRACE( T32, fd, T32, iovs, T32, iovs_len, T32, retptr0, TEND );

  if ( fd >= N_FDS ) {
    return __WASI_ERRNO_MFILE;
  }

  if ( fd == STDIN || fds[fd].open == false ) {
    return __WASI_ERRNO_BADF;
  }

  f = files[fds[fd].file_id];

  if ( f.is_dir ) {
    return __WASI_ERRNO_BADF;
  }

  for ( int32_t i = 0; i < iovs_len; i++ ) {
    iobuf_offset
      = get_i32_program( iovs + i * (int32_t)sizeof( __wasi_iovec_t ) + (int32_t)offsetof( __wasi_iovec_t, buf ) );
    iobuf_len = get_i32_program( iovs + i * (int32_t)sizeof( __wasi_iovec_t )
                                 + (int32_t)offsetof( __wasi_iovec_t, buf_len ) );

    while ( (int32_t)( fds[fd].offset + (unsigned long)iobuf_len )
            > page_size_rw_mem( f.mem_id ) * WASM_RT_PAGESIZE ) {
      grow_rw_mem_pages( f.mem_id, iobuf_len / WASM_RT_PAGESIZE + 1 );
    }
    program_mem_to_rw_mem( f.mem_id, (int32_t)fds[fd].offset, iobuf_offset, iobuf_len );
    fds[fd].offset += (unsigned long)iobuf_len;
    total_written += iobuf_len;
  }

  flatware_mem_to_program_mem( retptr0, (int32_t)&total_written, sizeof( total_written ) );
  RET_TRACE( total_written );
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

  if ( fd >= N_FDS ) {
    return __WASI_ERRNO_MFILE;
  }

  if ( fd < FILE_START || fds[fd].open == false )
    return __WASI_ERRNO_BADF;

  if ( !( fds[fd].stat.fs_rights_base & __WASI_RIGHTS_FD_FDSTAT_SET_FLAGS ) )
    return __WASI_ERRNO_PERM;

  fds[fd].stat.fs_flags = (__wasi_fdflags_t)fdflags;
  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Gets file descriptor information about preopened files.
 * @details No effect as no files are preopened. Returns BADF.
 *
 * @param fd File descriptor
 * @param retptr0 Returns file descriptor information as __wasi_prestat_t
 * @return int32_t Status code
 */
int32_t fd_prestat_get( int32_t fd, int32_t retptr0 )
{
  filedesc* f;
  __wasi_prestat_t prestat;

  FUNC_TRACE( T32, fd, T32, retptr0, TEND );

  if ( fd > N_FDS ) {
    return __WASI_ERRNO_MFILE;
  }

  if ( fd <= STDERR ) {
    return __WASI_ERRNO_PERM;
  }

  f = &fds[fd];
  if ( f->open == false ) {
    return __WASI_ERRNO_BADF;
  }

  prestat.tag = __WASI_PREOPENTYPE_DIR;
  prestat.u.dir.pr_name_len = files[f->file_id].name.len;
  flatware_mem_to_program_mem( retptr0, (int32_t)&prestat, sizeof( prestat ) );

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Gets path of preopened files.
 * @details No effect as no files are preopened. Returns BADF.
 *
 * @param fd File descriptor
 * @param path String buffer to store path
 * @param path_len Length of path buffer
 * @return int32_t Status code
 */
int32_t fd_prestat_dir_name( int32_t fd, int32_t path, int32_t path_len )
{
  filedesc* f;

  FUNC_TRACE( T32, fd, T32, path, T32, path_len, TEND );

  if ( fd <= STDERR ) {
    return __WASI_ERRNO_PERM;
  }

  if ( fd > N_FDS ) {
    return __WASI_ERRNO_MFILE;
  }

  f = &fds[fd];
  if ( f->open == false ) {
    return __WASI_ERRNO_BADF;
  }

  flatware_mem_to_program_mem( path, (int32_t)files[f->file_id].name.ptr, path_len );
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
  file f;
  int64_t grow_length;

  FUNC_TRACE( T32, fd, T64, offset, T64, len, TEND );

  if ( fd <= STDERR || fd >= N_FDS || fds[fd].open == false )
    return __WASI_ERRNO_BADF;

  f = files[fds[fd].file_id];
  grow_length = page_size_rw_mem( f.mem_id ) * WASM_RT_PAGESIZE - offset - len;

  if ( grow_length > 0 ) {
    grow_rw_mem_pages( f.mem_id, (int32_t)( grow_length / WASM_RT_PAGESIZE + 1 ) );
  }

  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Synchronizes the data of a file descriptor.
 *
 * @param fd File descriptor
 * @return int32_t Status code
 */
int32_t fd_datasync( int32_t fd )
{
  FUNC_TRACE( T32, fd, TEND );

  return 0;
}

/**
 * @brief Gets the attributes of a file descriptor.
 * @details Not supported yet. Returns NOTSUP.
 *
 * @param fd File descriptor
 * @param retptr0 Returns file descriptor attributes as __wasi_filestat_t
 * @return int32_t Status code
 */
int32_t fd_filestat_get( int32_t fd, int32_t retptr0 )
{
  __wasi_filestat_t stat;

  FUNC_TRACE( T32, fd, T32, retptr0, TEND );

  if ( fd >= N_FDS ) {
    return __WASI_ERRNO_MFILE;
  }

  if ( fds[fd].open == false ) {
    return __WASI_ERRNO_BADF;
  }

  if ( fds[fd].stat.fs_filetype == __WASI_FILETYPE_REGULAR_FILE ) {
    stat.dev = 1;
    stat.nlink = 1;
    stat.size = (unsigned long)fds[fd].size;
    stat.atim = 1757913961613440734;
    stat.mtim = 1757913961613440734;
    stat.ctim = 1757913961613440734;
    stat.filetype = __WASI_FILETYPE_REGULAR_FILE;
    flatware_mem_to_program_mem( retptr0, (int32_t)&stat, sizeof( stat ) );
    return 0;
  } else if ( fds[fd].stat.fs_filetype == __WASI_FILETYPE_DIRECTORY ) {
    stat.dev = 1;
    stat.size = 4096;
    stat.filetype = __WASI_FILETYPE_DIRECTORY;
    stat.atim = 1757913961613440734;
    stat.mtim = 1757913961613440734;
    stat.ctim = 1757913961613440734;
    flatware_mem_to_program_mem( retptr0, (int32_t)&stat, sizeof( stat ) );
    return 0;
  }

  return __WASI_ERRNO_NOTSUP;
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

  if ( fd >= N_FDS ) {
    return __WASI_ERRNO_MFILE;
  }

  if ( fd < FILE_START || fds[fd].open == false ) {
    return __WASI_ERRNO_BADF;
  }

  if ( fds[fd].stat.fs_rights_base & __WASI_RIGHTS_FD_FILESTAT_SET_SIZE ) {
    file f = files[fds[fd].file_id];
    int32_t grow_pages
      = (int32_t)( ( size - page_size_rw_mem( f.mem_id ) * WASM_RT_PAGESIZE ) / WASM_RT_PAGESIZE + 1 );
    fds[fd].size = (int32_t)size;
    f = files[fds[fd].file_id];
    if ( grow_pages > 0 ) {
      grow_rw_mem_pages( f.mem_id, grow_pages );
    }
    return __WASI_ERRNO_SUCCESS;
  }

  return __WASI_ERRNO_PERM;
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

  return 0;
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
  int32_t total_read = 0;
  int32_t iobuf_offset, iobuf_len;
  int64_t size_to_read;
  file f;

  FUNC_TRACE( T32, fd, T32, iovs, T32, iovs_len, T64, offset, T32, retptr0, TEND );

  if ( fd >= N_FDS ) {
    return __WASI_ERRNO_MFILE;
  }

  if ( ( fd < FILE_START && fd != STDIN ) || fds[fd].open == false ) {
    return __WASI_ERRNO_BADF;
  }

  f = files[fds[fd].file_id];

  if ( f.is_dir ) {
    return __WASI_ERRNO_BADF;
  }

  // Iterate over buffers
  for ( int32_t i = 0; i < iovs_len; ++i ) {
    int64_t file_remaining = fds[fd].size - offset;
    if ( file_remaining == 0 ) {
      break;
    }

    iobuf_offset
      = get_i32_program( iovs + i * (int32_t)sizeof( __wasi_iovec_t ) + (int32_t)offsetof( __wasi_iovec_t, buf ) );
    iobuf_len = get_i32_program( iovs + i * (int32_t)sizeof( __wasi_iovec_t )
                                 + (int32_t)offsetof( __wasi_iovec_t, buf_len ) );

    size_to_read = iobuf_len < file_remaining ? iobuf_len : file_remaining;
    ro_mem_to_program_mem( f.mem_id, iobuf_offset, (int32_t)offset, (int32_t)size_to_read );
    offset += size_to_read;
    total_read += size_to_read;
  }

  flatware_mem_to_program_mem( retptr0, (int32_t)&total_read, sizeof( total_read ) );
  RET_TRACE( total_read );
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

  return 0;
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
  struct substring path_str;
  unsigned long offset = 0;
  unsigned long buf_used = 0;
  char null = '\0';
  FUNC_TRACE( T32, fd, T32, buf, T32, buf_len, T64, cookie, T32, retptr0, TEND );

  path_str = files[fds[fd].file_id].name;

  if ( find_deep_entry( path_str, FileSystemBaseROTable, ScratchROTable ) == FILESYS_SEARCH_FAILED ) {
    return __WASI_ERRNO_NOENT;
  }

  if ( is_dir( ScratchROTable ) ) {
    attach_tree_ro_table( ScratchROTable, get_ro_table( ScratchROTable, DIRENT_CONTENT ) );
    for ( int i = 0; i < size_ro_table( ScratchROTable ); i++ ) {
      __wasi_dirent_t dirent;
      struct substring name;
      unsigned long offset_next;

      // attach flatware dirent
      attach_tree_ro_table( ScratchFileROTable, get_ro_table( ScratchROTable, i ) );

      name = get_name( ScratchFileROTable );

      if ( is_dir( ScratchFileROTable ) ) {
        dirent.d_type = __WASI_FILETYPE_DIRECTORY;
      } else {
        dirent.d_type = __WASI_FILETYPE_REGULAR_FILE;
      }

      dirent.d_namlen = name.len;

      offset_next = offset + sizeof( dirent ) + name.len + 1;
      dirent.d_next = offset_next;

      if ( offset >= cookie && offset_next < cookie + (unsigned long)buf_len ) {
        // Copy the content to buf
        flatware_mem_to_program_mem(
          ( buf + (int32_t)offset - (int32_t)cookie ), (int32_t)&dirent, sizeof( dirent ) );
        flatware_mem_to_program_mem( ( buf + (int32_t)offset - (int32_t)cookie + (int32_t)sizeof( dirent ) ),
                                     (int32_t)name.ptr,
                                     (int32_t)name.len );
        flatware_mem_to_program_mem(
          ( buf + (int32_t)offset - (int32_t)cookie + (int32_t)sizeof( dirent ) + (int32_t)name.len ),
          (int32_t)&null,
          1 );

      } else if ( offset_next >= cookie + (unsigned long)buf_len ) {
        buf_used = offset - (unsigned long)cookie;
        flatware_mem_to_program_mem( retptr0, (int32_t)&buf_used, sizeof( buf_used ) );
        return 0;
      }
      offset = offset_next;
    }
  }

  buf_used = offset - (unsigned long)cookie;
  flatware_mem_to_program_mem( retptr0, (int32_t)&buf_used, sizeof( buf_used ) );
  return 0;
}

/**
 * @brief Synchronizes file and metadata changes to storage.
 *
 * @param fd File descriptor
 * @return int32_t Status code
 */
int32_t fd_sync( int32_t fd )
{
  FUNC_TRACE( T32, fd, TEND );

  return 0;
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

  if ( fd >= N_FDS ) {
    return __WASI_ERRNO_MFILE;
  }

  if ( fd < FILE_START || fds[fd].open == false ) {
    return __WASI_ERRNO_BADF;
  }

  flatware_mem_to_program_mem( retptr0, (int32_t)&fds[fd].offset, sizeof( fds[fd].offset ) );
  RET_TRACE( fds[fd].offset );

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

  return 0;
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
  struct substring path_str;
  __wasi_filestat_t stat;

  FUNC_TRACE( T32, fd, T32, flags, T32, path, T32, path_len, T32, retptr0, TEND );

  path_str.len = (size_t)path_len;
  path_str.ptr = (char*)calloc( path_str.len + 1, sizeof( char ) );
  program_mem_to_flatware_mem( (int32_t)path_str.ptr, (int32_t)path, (int32_t)path_str.len );

  if ( find_deep_entry( path_str, FileSystemBaseROTable, ScratchROTable ) == FILESYS_SEARCH_FAILED ) {
    return __WASI_ERRNO_NOENT;
  }

  if ( is_dir( ScratchROTable ) ) {
    stat.dev = 1;
    stat.size = 4096;
    stat.atim = 1757913961613440734;
    stat.mtim = 1757913961613440734;
    stat.ctim = 1757913961613440734;
    stat.filetype = __WASI_FILETYPE_DIRECTORY;
    flatware_mem_to_program_mem( retptr0, (int32_t)&stat, sizeof( stat ) );
    return __WASI_ERRNO_SUCCESS;
  } else {
    attach_blob_ro_mem( ScratchROMem, get_ro_table( ScratchROTable, DIRENT_CONTENT ) );
    stat.dev = 1;
    stat.nlink = 1;
    stat.atim = 1757913961613440734;
    stat.mtim = 1757913961613440734;
    stat.ctim = 1757913961613440734;
    stat.size = (unsigned long)byte_size_ro_mem( ScratchROMem );
    stat.filetype = __WASI_FILETYPE_REGULAR_FILE;
    flatware_mem_to_program_mem( retptr0, (int32_t)&stat, sizeof( stat ) );
    return 0;
  }
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

  return 0;
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

  num = size_ro_table( ArgsROTable );

  flatware_mem_to_program_mem( num_argument_ptr, (int32_t)&num, 4 );
  RET_TRACE( num );

  // Actual arguments
  for ( int32_t i = 0; i < num; i++ ) {
    attach_blob_ro_mem( ScratchROMem, get_ro_table( ArgsROTable, i ) );
    size += byte_size_ro_mem( ScratchROMem );
    size += 1;
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
  char null = '\0';
  int32_t size = 0;
  int32_t addr = argv_buf_ptr;

  FUNC_TRACE( T32, argv_ptr, T32, argv_buf_ptr, TEND );
  for ( int32_t i = 0; i < size_ro_table( ArgsROTable ); i++ ) {
    attach_blob_ro_mem( ScratchROMem, get_ro_table( ArgsROTable, i ) );
    size = byte_size_ro_mem( ScratchROMem );
    flatware_mem_to_program_mem( argv_ptr + i * (int32_t)sizeof( char* ), (int32_t)&addr, sizeof( char* ) );
    ro_mem_to_program_mem( ScratchROMem, addr, 0, size );
    flatware_mem_to_program_mem( addr + size, (int32_t)&null, 1 );
    addr += size + 1;
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
  int32_t size = 0;
  int32_t num = size_ro_table( EnvROTable );

  FUNC_TRACE( T32, retptr0, T32, retptr1, TEND );

  for ( int32_t i = 0; i < num; i++ ) {
    attach_blob_ro_mem( ScratchROMem, get_ro_table( EnvROTable, i ) );
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
  int32_t size = 0;
  int32_t addr = environ_buf;

  FUNC_TRACE( T32, environ, T32, environ_buf, TEND );

  for ( int32_t i = 0; i < size_ro_table( EnvROTable ); i++ ) {
    attach_blob_ro_mem( ScratchROMem, get_ro_table( EnvROTable, i ) );
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
  struct substring path_str;

  __wasi_fd_t result_fd = FILE_START;
  int32_t file_id = -1;     // File id
  int32_t file_mem_id = -1; // File memory index

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

  while ( result_fd < N_FDS && fds[result_fd].open ) {
    result_fd++;
  }

  if ( result_fd >= N_FDS ) {
    return __WASI_ERRNO_NFILE;
  }

  path_str.len = (size_t)path_len;
  path_str.ptr = (char*)calloc( path_str.len + 1, sizeof( char ) );
  program_mem_to_flatware_mem( (int32_t)path_str.ptr, (int32_t)path, (int32_t)path_str.len );

  for ( int32_t i = 0; i < N_FILES; ++i ) {
    if ( files[i].name.len == path_str.len && strncmp( files[i].name.ptr, path_str.ptr, path_str.len ) == 0 ) {
      file_id = i;
      file_mem_id = files[i].mem_id;
      break;
    }
  }

  if ( file_id == -1 ) {
    if ( find_deep_entry( path_str, FileSystemBaseROTable, ScratchROTable ) == FILESYS_SEARCH_FAILED ) {
      return __WASI_ERRNO_NOENT;
    }

    for ( int32_t i = 0; i < N_FILES; ++i ) {
      if ( !files[i].in_use ) {
        file_id = i;
        files[i].in_use = true;
        break;
      }
    }

    if ( file_id == -1 ) {
      return __WASI_ERRNO_NFILE;
    }

    files[file_id].num_fds = 0;
    files[file_id].name = path_str;

    if ( is_dir( ScratchROTable ) ) {
      files[file_id].is_dir = true;
      files[file_id].mem_id = -1;
    } else {
      files[file_id].is_dir = false;

      for ( int32_t i = File0ROMem; i < NUM_RO_MEM; ++i ) {
        if ( !bitset_get( ro_mem_use, (uint32_t)i ) ) {
          file_mem_id = i;
          bitset_set( ro_mem_use, (uint32_t)i );
          break;
        }
      }

      if ( file_mem_id == -1 ) {
        return __WASI_ERRNO_NFILE;
      }

      attach_blob_ro_mem( file_mem_id, get_ro_table( ScratchROTable, DIRENT_CONTENT ) );
      files[file_id].mem_id = file_mem_id;
    }
  }

  files[file_id].num_fds++;

  fds[result_fd].offset = 0;
  fds[result_fd].size = files[file_id].is_dir ? 4096 : byte_size_ro_mem( file_mem_id );
  fds[result_fd].open = true;
  fds[result_fd].file_id = file_id;

  if ( files[file_id].is_dir ) {
    fds[result_fd].stat.fs_filetype = __WASI_FILETYPE_DIRECTORY;
    fds[result_fd].stat.fs_flags = 0;
    fds[result_fd].stat.fs_rights_base = __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_WRITE | __WASI_RIGHTS_FD_READDIR;
    fds[result_fd].stat.fs_rights_inheriting
      = __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_WRITE | __WASI_RIGHTS_FD_READDIR;
  } else {
    fds[result_fd].stat.fs_filetype = __WASI_FILETYPE_REGULAR_FILE;
    fds[result_fd].stat.fs_flags = __WASI_FDFLAGS_APPEND;
    fds[result_fd].stat.fs_rights_base = __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_SEEK;
    fds[result_fd].stat.fs_rights_inheriting = __WASI_RIGHTS_FD_READ | __WASI_RIGHTS_FD_SEEK;
  }

  flatware_mem_to_program_mem( retptr0, (int32_t)&result_fd, sizeof( result_fd ) );
  RET_TRACE( result_fd );
  return __WASI_ERRNO_SUCCESS;
}

/**
 * @brief Gets the resolution of a clock.
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

  return 0;
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

/**
 * @brief Gets the value of the bitset at the given index.
 *
 * @param bitset Bitset
 * @param index Index
 * @return true Index is set
 * @return false Index isn't set
 */
static bool bitset_get( unsigned int* bitset, uint32_t index )
{
  return ( bitset[index / sizeof( unsigned int )] >> ( index % sizeof( unsigned int ) ) ) & 1;
}

/**
 * @brief Sets the value of the bitset at the given index.
 *
 * @param bitset Bitset
 * @param index Index
 */
static void bitset_set( unsigned int* bitset, uint32_t index )
{
  bitset[index / sizeof( unsigned int )] |= 1 << ( index % sizeof( unsigned int ) );
}

/**
 * @brief Clears the value of the bitset at the given index.
 *
 * @param bitset Bitset
 * @param index Index
 */
static void bitset_clear( unsigned int* bitset, uint32_t index )
{
  bitset[index / sizeof( unsigned int )] &= ~( 1 << ( index % sizeof( unsigned int ) ) );
}

externref fixpoint_apply( externref encode )
{
  ro_mem_use
    = (unsigned int*)calloc( ( NUM_RO_MEM - File0ROMem ) / sizeof( unsigned int ), sizeof( unsigned int ) );

  attach_tree_ro_table( InputROTable, encode );

  attach_tree_ro_table( FileSystemBaseROTable, get_ro_table( InputROTable, INPUT_FILESYSTEM ) );
  attach_tree_ro_table( ArgsROTable, get_ro_table( InputROTable, INPUT_ARGS ) );
  attach_blob_ro_mem( StdInROMem, get_ro_table( InputROTable, INPUT_STDIN ) );
  fds[STDIN].size = byte_size_ro_mem( StdInROMem );
  attach_tree_ro_table( EnvROTable, get_ro_table( InputROTable, INPUT_ENV ) );

  attach_blob_ro_mem( ScratchROMem, get_ro_table( InputROTable, INPUT_RANDOM_SEED ) );
  random_seed = get_i32_ro_mem( ScratchROMem, 0 );

  grow_rw_table( OutputRWTable, 5, create_blob_i32( 0 ) );

  run_start();

  set_rw_table( OutputRWTable,
                OUTPUT_FILESYSTEM,
                create_tree_rw_table( FileSystemRWTable, size_rw_table( FileSystemRWTable ) ) );
  set_rw_table( OutputRWTable, OUTPUT_STDOUT, create_blob_rw_mem( StdOutRWMem, (int32_t)fds[STDOUT].offset ) );
  set_rw_table( OutputRWTable, OUTPUT_STDERR, create_blob_rw_mem( StdErrRWMem, (int32_t)fds[STDERR].offset ) );
  set_rw_table( OutputRWTable, OUTPUT_TRACE, create_blob_rw_mem( StdTraceRWMem, trace_offset ) );

  free( ro_mem_use );

  return create_tree_rw_table( OutputRWTable, 5 );
}

#pragma clang diagnostic pop
