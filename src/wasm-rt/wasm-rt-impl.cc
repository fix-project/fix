/*
 * Copyright 2018 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "wasm-rt-impl.hh"

#include <assert.h>
#include <cstddef>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if WASM_RT_MEMCHECK_SIGNAL_HANDLER && !WASM_RT_SKIP_SIGNAL_RECOVERY && !defined( _WIN32 )
#include <signal.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#if _MSC_VER
#include <malloc.h>
#define alloca _alloca
#endif

#define MAX_EXCEPTION_SIZE WASM_RT_PAGE_SIZE

#if WASM_RT_MEMCHECK_SIGNAL_HANDLER && !WASM_RT_SKIP_SIGNAL_RECOVERY
static bool g_signal_handler_installed = false;
#ifdef _WIN32
static void* g_sig_handler_handle = 0;
#else
static char* g_alt_stack = 0;
#endif
#endif

#if WASM_RT_USE_STACK_DEPTH_COUNT
WASM_RT_THREAD_LOCAL uint32_t wasm_rt_call_stack_depth;
WASM_RT_THREAD_LOCAL uint32_t wasm_rt_saved_call_stack_depth;
#endif

WASM_RT_THREAD_LOCAL wasm_rt_jmp_buf g_wasm_rt_jmp_buf;

static WASM_RT_THREAD_LOCAL wasm_rt_tag_t g_active_exception_tag;
static WASM_RT_THREAD_LOCAL uint8_t g_active_exception[MAX_EXCEPTION_SIZE];
static WASM_RT_THREAD_LOCAL uint32_t g_active_exception_size;

static WASM_RT_THREAD_LOCAL wasm_rt_jmp_buf* g_unwind_target;

void wasm_rt_trap( wasm_rt_trap_t code )
{
  assert( code != WASM_RT_TRAP_NONE );
#if WASM_RT_USE_STACK_DEPTH_COUNT
  wasm_rt_call_stack_depth = wasm_rt_saved_call_stack_depth;
#endif

#ifdef WASM_RT_TRAP_HANDLER
  WASM_RT_TRAP_HANDLER( code );
  wasm_rt_unreachable();
#else
  WASM_RT_LONGJMP( g_wasm_rt_jmp_buf, code );
#endif
}

void wasm_rt_load_exception( const wasm_rt_tag_t tag, uint32_t size, const void* values )
{
  if ( size > MAX_EXCEPTION_SIZE ) {
    wasm_rt_trap( WASM_RT_TRAP_EXHAUSTION );
  }

  g_active_exception_tag = tag;
  g_active_exception_size = size;

  if ( size ) {
    memcpy( g_active_exception, values, size );
  }
}

WASM_RT_NO_RETURN void wasm_rt_throw( void )
{
  WASM_RT_LONGJMP( *g_unwind_target, WASM_RT_TRAP_UNCAUGHT_EXCEPTION );
}

WASM_RT_UNWIND_TARGET* wasm_rt_get_unwind_target( void )
{
  return g_unwind_target;
}

void wasm_rt_set_unwind_target( WASM_RT_UNWIND_TARGET* target )
{
  g_unwind_target = target;
}

wasm_rt_tag_t wasm_rt_exception_tag( void )
{
  return g_active_exception_tag;
}

uint32_t wasm_rt_exception_size( void )
{
  return g_active_exception_size;
}

void* wasm_rt_exception( void )
{
  return g_active_exception;
}

#ifdef _WIN32
static void* os_mmap( size_t size )
{
  void* ret = VirtualAlloc( NULL, size, MEM_RESERVE, PAGE_NOACCESS );
  return ret;
}

static int os_munmap( void* addr, size_t size )
{
  // Windows can only unmap the whole mapping
  (void)size; /* unused */
  BOOL succeeded = VirtualFree( addr, 0, MEM_RELEASE );
  return succeeded ? 0 : -1;
}

static int os_mprotect( void* addr, size_t size )
{
  if ( size == 0 ) {
    return 0;
  }
  void* ret = VirtualAlloc( addr, size, MEM_COMMIT, PAGE_READWRITE );
  if ( ret == addr ) {
    return 0;
  }
  VirtualFree( addr, 0, MEM_RELEASE );
  return -1;
}

static void os_print_last_error( const char* msg )
{
  DWORD errorMessageID = GetLastError();
  if ( errorMessageID != 0 ) {
    LPSTR messageBuffer = 0;
    // The api creates the buffer that holds the message
    size_t size
      = FormatMessageA( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                        NULL,
                        errorMessageID,
                        MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
                        (LPSTR)&messageBuffer,
                        0,
                        NULL );
    (void)size;
    printf( "%s. %s\n", msg, messageBuffer );
    LocalFree( messageBuffer );
  } else {
    printf( "%s. No error code.\n", msg );
  }
}

#if WASM_RT_MEMCHECK_SIGNAL_HANDLER && !WASM_RT_SKIP_SIGNAL_RECOVERY

static LONG os_signal_handler( PEXCEPTION_POINTERS info )
{
  if ( info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ) {
    wasm_rt_trap( WASM_RT_TRAP_OOB );
  } else if ( info->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW ) {
    wasm_rt_trap( WASM_RT_TRAP_EXHAUSTION );
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

static void os_install_signal_handler( void )
{
  g_sig_handler_handle = AddVectoredExceptionHandler( 1 /* CALL_FIRST */, os_signal_handler );
}

static void os_cleanup_signal_handler( void )
{
  RemoveVectoredExceptionHandler( g_sig_handler_handle );
}

#endif

#else
static void* os_mmap( size_t size )
{
  int map_prot = PROT_NONE;
  int map_flags = MAP_ANONYMOUS | MAP_PRIVATE;
  uint8_t* addr = static_cast<uint8_t*>( mmap( NULL, size, map_prot, map_flags, -1, 0 ) );
  if ( addr == MAP_FAILED )
    return NULL;
  return addr;
}

static int os_munmap( void* addr, size_t size )
{
  return munmap( addr, size );
}

static int os_mprotect( void* addr, size_t size )
{
  return mprotect( addr, size, PROT_READ | PROT_WRITE );
}

static void os_print_last_error( const char* msg )
{
  perror( msg );
}

#if WASM_RT_MEMCHECK_SIGNAL_HANDLER && !WASM_RT_SKIP_SIGNAL_RECOVERY
static void os_signal_handler( __attribute__( ( unused ) ) int sig,
                               siginfo_t* si,
                               __attribute__( ( unused ) ) void* unused )
{
  if ( si->si_code == SEGV_ACCERR ) {
    wasm_rt_trap( WASM_RT_TRAP_OOB );
  } else {
    wasm_rt_trap( WASM_RT_TRAP_EXHAUSTION );
  }
}

static void os_install_signal_handler( void )
{
  /* Use alt stack to handle SIGSEGV from stack overflow */
  g_alt_stack = static_cast<char*>( malloc( SIGSTKSZ ) );
  if ( g_alt_stack == NULL ) {
    perror( "malloc failed" );
    abort();
  }

  stack_t ss;
  ss.ss_sp = g_alt_stack;
  ss.ss_flags = 0;
  ss.ss_size = SIGSTKSZ;
  if ( sigaltstack( &ss, NULL ) != 0 ) {
    perror( "sigaltstack failed" );
    abort();
  }

  struct sigaction sa;
  memset( &sa, '\0', sizeof( sa ) );
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
  sigemptyset( &sa.sa_mask );
  sa.sa_sigaction = os_signal_handler;

  /* Install SIGSEGV and SIGBUS handlers, since macOS seems to use SIGBUS. */
  if ( sigaction( SIGSEGV, &sa, NULL ) != 0 || sigaction( SIGBUS, &sa, NULL ) != 0 ) {
    perror( "sigaction failed" );
    abort();
  }
}

static void os_cleanup_signal_handler( void )
{
  /* Undo what was done in os_install_signal_handler */
  struct sigaction sa;
  memset( &sa, '\0', sizeof( sa ) );
  sa.sa_handler = SIG_DFL;
  if ( sigaction( SIGSEGV, &sa, NULL ) != 0 || sigaction( SIGBUS, &sa, NULL ) ) {
    perror( "sigaction failed" );
    abort();
  }

  if ( sigaltstack( NULL, NULL ) != 0 ) {
    perror( "sigaltstack failed" );
    abort();
  }

  free( g_alt_stack );
}
#endif

#endif

void wasm_rt_init( void )
{
#if WASM_RT_MEMCHECK_SIGNAL_HANDLER && !WASM_RT_SKIP_SIGNAL_RECOVERY
  if ( !g_signal_handler_installed ) {
    g_signal_handler_installed = true;
    os_install_signal_handler();
  }
#endif
}

bool wasm_rt_is_initialized( void )
{
#if WASM_RT_MEMCHECK_SIGNAL_HANDLER && !WASM_RT_SKIP_SIGNAL_RECOVERY
  return g_signal_handler_installed;
#else
  return true;
#endif
}

void wasm_rt_free( void )
{
#if WASM_RT_MEMCHECK_SIGNAL_HANDLER && !WASM_RT_SKIP_SIGNAL_RECOVERY
  os_cleanup_signal_handler();
#endif
}

void wasm_rt_allocate_memory_helper( wasm_rt_memory_t* memory,
                                     uint64_t initial_pages,
                                     uint64_t max_pages,
                                     bool is64,
                                     bool hw_checked )
{
  memory->read_only = false;
  if ( initial_pages == 0 ) {
    memory->data = NULL;
    memory->size = 0;
    memory->pages = initial_pages;
    memory->max_pages = max_pages;
    return;
  }

  uint64_t byte_length = initial_pages * WASM_RT_PAGE_SIZE;
  if ( hw_checked ) {
    /* Reserve 8GiB. */
    assert( !is64 && "memory64 is not yet compatible with WASM_RT_MEMCHECK_SIGNAL_HANDLER" );
    void* addr = os_mmap( 0x200000000ul );

    if ( !addr ) {
      os_print_last_error( "os_mmap failed." );
      abort();
    }
    int ret = os_mprotect( addr, byte_length );
    if ( ret != 0 ) {
      os_print_last_error( "os_mprotect failed." );
      abort();
    }
    memory->data = static_cast<uint8_t*>( addr );
  } else {
    memory->data = static_cast<uint8_t*>( calloc( byte_length, 1 ) );
  }
  memory->size = byte_length;
  memory->pages = initial_pages;
  memory->max_pages = max_pages;
  memory->is64 = is64;
}

void wasm_rt_allocate_memory_sw_checked( wasm_rt_memory_t* memory,
                                         uint64_t initial_pages,
                                         uint64_t max_pages,
                                         bool is64 )
{
  wasm_rt_allocate_memory_helper( memory, initial_pages, max_pages, is64, false );
}

void wasm_rt_allocate_memory( wasm_rt_memory_t* memory, uint64_t initial_pages, uint64_t max_pages, bool is64 )
{
  wasm_rt_allocate_memory_helper( memory, initial_pages, max_pages, is64, WASM_RT_MEMCHECK_SIGNAL_HANDLER );
}

uint64_t wasm_rt_grow_memory_helper( wasm_rt_memory_t* memory, uint64_t delta, bool hw_checked )
{
  uint64_t old_pages = memory->pages;
  uint64_t new_pages = memory->pages + delta;
  if ( new_pages == 0 ) {
    return 0;
  }
  if ( new_pages < old_pages || new_pages > memory->max_pages ) {
    return (uint64_t)-1;
  }
  uint64_t old_size = old_pages * WASM_RT_PAGE_SIZE;
  uint64_t new_size = new_pages * WASM_RT_PAGE_SIZE;
  uint64_t delta_size = delta * WASM_RT_PAGE_SIZE;
  uint8_t* new_data;
  if ( hw_checked ) {
    new_data = memory->data;
    int ret = os_mprotect( new_data + old_size, delta_size );
    if ( ret != 0 ) {
      return (uint64_t)-1;
    }
  } else {
    new_data = static_cast<uint8_t*>( realloc( memory->data, new_size ) );
    if ( new_data == NULL ) {
      return (uint64_t)-1;
    }

#if !WABT_BIG_ENDIAN
    memset( new_data + old_size, 0, delta_size );
#endif
  }
#if WABT_BIG_ENDIAN
  memmove( new_data + new_size - old_size, new_data, old_size );
  memset( new_data, 0, delta_size );
#endif
  memory->pages = new_pages;
  memory->size = new_size;
  memory->data = new_data;
  return old_pages;
}

uint64_t wasm_rt_grow_memory_sw_checked( wasm_rt_memory_t* memory, uint64_t delta )
{
  return wasm_rt_grow_memory_helper( memory, delta, false );
}

uint64_t wasm_rt_grow_memory( wasm_rt_memory_t* memory, uint64_t delta )
{
  return wasm_rt_grow_memory_helper( memory, delta, WASM_RT_MEMCHECK_SIGNAL_HANDLER );
}

void wasm_rt_free_memory_hw_checked( wasm_rt_memory_t* memory )
{
  os_munmap( memory->data, memory->size ); // ignore error?
}

void wasm_rt_free_memory_sw_checked( wasm_rt_memory_t* memory )
{
  if ( memory->read_only )
    return;
  free( memory->data );
}

void wasm_rt_free_memory( wasm_rt_memory_t* memory )
{
#if WASM_RT_MEMCHECK_SIGNAL_HANDLER
  wasm_rt_free_memory_hw_checked( memory );
#else
  wasm_rt_free_memory_sw_checked( memory );
#endif
}

#define DEFINE_TABLE_OPS( type )                                                                                   \
  void wasm_rt_allocate_##type##_table(                                                                            \
    wasm_rt_##type##_table_t* table, uint32_t elements, uint32_t max_elements )                                    \
  {                                                                                                                \
    table->read_only = false;                                                                                      \
    table->size = elements;                                                                                        \
    table->max_size = max_elements;                                                                                \
    table->data = NULL;                                                                                            \
    if ( table->size != 0 ) {                                                                                      \
      void* ptr = aligned_alloc( alignof( wasm_rt_##type##_t ), table->size * sizeof( wasm_rt_##type##_t ) );      \
      table->data = static_cast<wasm_rt_##type##_t*>( ptr );                                                       \
      for ( int i = 0; i < (int32_t)table->size; i++ ) {                                                           \
        table->data[i] = wasm_rt_##type##_null_value;                                                              \
      }                                                                                                            \
    }                                                                                                              \
  }                                                                                                                \
  void wasm_rt_free_##type##_table( wasm_rt_##type##_table_t* table )                                              \
  {                                                                                                                \
    if ( table->read_only )                                                                                        \
      return;                                                                                                      \
    free( table->data );                                                                                           \
  }                                                                                                                \
  uint32_t wasm_rt_grow_##type##_table( wasm_rt_##type##_table_t* table, uint32_t delta, wasm_rt_##type##_t init ) \
  {                                                                                                                \
    wasm_rt_##type##_t* old_data = table->data;                                                                    \
    uint32_t old_elems = table->size;                                                                              \
    uint64_t new_elems = (uint64_t)table->size + delta;                                                            \
    if ( new_elems == 0 ) {                                                                                        \
      return 0;                                                                                                    \
    }                                                                                                              \
    if ( ( new_elems < old_elems ) || ( new_elems > table->max_size ) ) {                                          \
      return (uint32_t)-1;                                                                                         \
    }                                                                                                              \
    void* ptr = aligned_alloc( alignof( wasm_rt_##type##_t ), new_elems * sizeof( wasm_rt_##type##_t ) );          \
    if ( ptr == NULL ) {                                                                                           \
      return -1;                                                                                                   \
    }                                                                                                              \
    table->data = static_cast<wasm_rt_##type##_t*>( ptr );                                                         \
    memcpy( table->data, old_data, old_elems * sizeof( wasm_rt_##type##_t ) );                                     \
    table->size = new_elems;                                                                                       \
    for ( uint32_t i = old_elems; i < new_elems; i++ ) {                                                           \
      table->data[i] = init;                                                                                       \
    }                                                                                                              \
    free( old_data );                                                                                              \
    return old_elems;                                                                                              \
  }

DEFINE_TABLE_OPS( funcref )
DEFINE_TABLE_OPS( externref )

const char* wasm_rt_strerror( wasm_rt_trap_t trap )
{
  switch ( trap ) {
    case WASM_RT_TRAP_NONE:
      return "No error";
    case WASM_RT_TRAP_OOB:
#if WASM_RT_MERGED_OOB_AND_EXHAUSTION_TRAPS
      return "Out-of-bounds access in linear memory or a table, or call stack "
             "exhausted";
#else
      return "Out-of-bounds access in linear memory or a table";
    case WASM_RT_TRAP_EXHAUSTION:
      return "Call stack exhausted";
#endif
    case WASM_RT_TRAP_INT_OVERFLOW:
      return "Integer overflow on divide or truncation";
    case WASM_RT_TRAP_DIV_BY_ZERO:
      return "Integer divide by zero";
    case WASM_RT_TRAP_INVALID_CONVERSION:
      return "Conversion from NaN to integer";
    case WASM_RT_TRAP_UNREACHABLE:
      return "Unreachable instruction executed";
    case WASM_RT_TRAP_CALL_INDIRECT:
      return "Invalid call_indirect";
    case WASM_RT_TRAP_UNCAUGHT_EXCEPTION:
      return "Uncaught exception";
  }
  return "invalid trap code";
}
