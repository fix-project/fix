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
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if WASM_RT_MEMCHECK_SIGNAL_HANDLER_POSIX
#include <signal.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#define PAGE_SIZE 65536
#define MAX_EXCEPTION_SIZE PAGE_SIZE

#include <stdexcept>

typedef struct FuncType
{
  wasm_rt_type_t* params;
  wasm_rt_type_t* results;
  uint32_t param_count;
  uint32_t result_count;
} FuncType;

#if WASM_RT_MEMCHECK_SIGNAL_HANDLER
static bool g_signal_handler_installed = false;
static char* g_alt_stack;
#else
uint32_t wasm_rt_call_stack_depth;
uint32_t wasm_rt_saved_call_stack_depth;
#endif

static FuncType* g_func_types;
static uint32_t g_func_type_count;

jmp_buf wasm_rt_jmp_buf;

static uint32_t g_active_exception_tag;
static uint8_t g_active_exception[MAX_EXCEPTION_SIZE];
static uint32_t g_active_exception_size;

static jmp_buf* g_unwind_target;

void wasm_rt_trap( wasm_rt_trap_t code )
{
  assert( code != WASM_RT_TRAP_NONE );
  throw std::runtime_error( wasm_rt_strerror( code ) );
}

static bool func_types_are_equal( FuncType* a, FuncType* b )
{
  if ( a->param_count != b->param_count || a->result_count != b->result_count )
    return 0;
  uint32_t i;
  for ( i = 0; i < a->param_count; ++i )
    if ( a->params[i] != b->params[i] )
      return 0;
  for ( i = 0; i < a->result_count; ++i )
    if ( a->results[i] != b->results[i] )
      return 0;
  return 1;
}

uint32_t wasm_rt_register_func_type( uint32_t param_count, uint32_t result_count, ... )
{
  FuncType func_type;
  func_type.param_count = param_count;
  func_type.params = static_cast<wasm_rt_type_t*>( malloc( param_count * sizeof( wasm_rt_type_t ) ) );
  func_type.result_count = result_count;
  func_type.results = static_cast<wasm_rt_type_t*>( malloc( result_count * sizeof( wasm_rt_type_t ) ) );

  va_list args;
  va_start( args, result_count );

  uint32_t i;
  for ( i = 0; i < param_count; ++i )
    func_type.params[i] = static_cast<wasm_rt_type_t>( va_arg( args, int ) );
  for ( i = 0; i < result_count; ++i )
    func_type.results[i] = static_cast<wasm_rt_type_t>( va_arg( args, int ) );
  va_end( args );

  for ( i = 0; i < g_func_type_count; ++i ) {
    if ( func_types_are_equal( &g_func_types[i], &func_type ) ) {
      free( func_type.params );
      free( func_type.results );
      return i + 1;
    }
  }

  uint32_t idx = g_func_type_count++;
  g_func_types = static_cast<FuncType*>( realloc( g_func_types, g_func_type_count * sizeof( FuncType ) ) );
  g_func_types[idx] = func_type;
  return idx + 1;
}

uint32_t wasm_rt_register_tag( uint32_t size )
{
  static uint32_t s_tag_count = 0;

  if ( size > MAX_EXCEPTION_SIZE ) {
    wasm_rt_trap( WASM_RT_TRAP_EXHAUSTION );
  }
  return s_tag_count++;
}

void wasm_rt_load_exception( uint32_t tag, uint32_t size, const void* values )
{
  assert( size <= MAX_EXCEPTION_SIZE );

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

uint32_t wasm_rt_exception_tag( void )
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

#if WASM_RT_MEMCHECK_SIGNAL_HANDLER_POSIX
static void signal_handler( int, siginfo_t* si, void* )
{
  if ( si->si_code == SEGV_ACCERR ) {
    wasm_rt_trap( WASM_RT_TRAP_OOB );
  } else {
    wasm_rt_trap( WASM_RT_TRAP_EXHAUSTION );
  }
}
#endif

#ifdef _WIN32
static void* os_mmap( size_t size )
{
  return VirtualAlloc( NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS );
}

static int os_munmap( void* addr, size_t size )
{
  BOOL succeeded = VirtualFree( addr, size, MEM_RELEASE );
  return succeeded ? 0 : -1;
}

static int os_mprotect( void* addr, size_t size )
{
  DWORD old;
  BOOL succeeded = VirtualProtect( (LPVOID)addr, size, PAGE_READWRITE, &old );
  return succeeded ? 0 : -1;
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
#endif

void wasm_rt_init( void )
{
#if WASM_RT_MEMCHECK_SIGNAL_HANDLER_POSIX
  if ( !g_signal_handler_installed ) {
    g_signal_handler_installed = true;

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
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset( &sa.sa_mask );
    sa.sa_sigaction = signal_handler;

    /* Install SIGSEGV and SIGBUS handlers, since macOS seems to use SIGBUS. */
    if ( sigaction( SIGSEGV, &sa, NULL ) != 0 || sigaction( SIGBUS, &sa, NULL ) != 0 ) {
      perror( "sigaction failed" );
      abort();
    }
  }
#endif
}

bool wasm_rt_is_initialized( void )
{
#if WASM_RT_MEMCHECK_SIGNAL_HANDLER_POSIX
  return g_signal_handler_installed;
#else
  return true;
#endif
}

void wasm_rt_free( void )
{
#if WASM_RT_MEMCHECK_SIGNAL_HANDLER_POSIX
  free( g_alt_stack );
#endif
}

void wasm_rt_allocate_memory_helper( wasm_rt_memory_t* memory,
                                     uint32_t initial_pages,
                                     uint32_t max_pages,
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

  uint32_t byte_length = initial_pages * PAGE_SIZE;

  if ( hw_checked ) {
    /* Reserve 8GiB. */
    void* addr = os_mmap( 0x200000000ul );

    if ( addr == (void*)-1 ) {
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
}

void wasm_rt_allocate_memory_sw_checked( wasm_rt_memory_t* memory, uint32_t initial_pages, uint32_t max_pages )
{
  wasm_rt_allocate_memory_helper( memory, initial_pages, max_pages, false );
}

void wasm_rt_allocate_memory( wasm_rt_memory_t* memory, uint32_t initial_pages, uint32_t max_pages )
{
  wasm_rt_allocate_memory_helper( memory, initial_pages, max_pages, WASM_RT_MEMCHECK_SIGNAL_HANDLER_POSIX );
}

uint32_t wasm_rt_grow_memory_helper( wasm_rt_memory_t* memory, uint32_t delta, bool hw_checked )
{
  uint32_t old_pages = memory->pages;
  uint32_t new_pages = memory->pages + delta;
  if ( new_pages == 0 ) {
    return 0;
  }
  if ( new_pages < old_pages || new_pages > memory->max_pages ) {
    return (uint32_t)-1;
  }
  uint32_t old_size = old_pages * PAGE_SIZE;
  uint32_t new_size = new_pages * PAGE_SIZE;
  uint32_t delta_size = delta * PAGE_SIZE;
  uint8_t* new_data;
  if ( hw_checked ) {
    new_data = memory->data;
    int ret = os_mprotect( new_data + old_size, delta_size );
    if ( ret != 0 ) {
      return (uint32_t)-1;
    }
  } else {
    new_data = static_cast<uint8_t*>( realloc( memory->data, new_size ) );
    if ( new_data == NULL ) {
      return (uint32_t)-1;
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

uint32_t wasm_rt_grow_memory_sw_checked( wasm_rt_memory_t* memory, uint32_t delta )
{
  return wasm_rt_grow_memory_helper( memory, delta, false );
}

uint32_t wasm_rt_grow_memory( wasm_rt_memory_t* memory, uint32_t delta )
{
  return wasm_rt_grow_memory_helper( memory, delta, WASM_RT_MEMCHECK_SIGNAL_HANDLER_POSIX );
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
#if WASM_RT_MEMCHECK_SIGNAL_HANDLER_POSIX
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
    if ( table->size != 0 ) {                                                                                      \
      void* ptr = aligned_alloc( alignof( wasm_rt_##type##_t ), table->size * sizeof( wasm_rt_##type##_t ) );      \
      memset( ptr, 0, table->size * sizeof( wasm_rt_##type##_t ) );                                                \
      table->data = static_cast<wasm_rt_##type##_t*>( ptr );                                                       \
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
    uint32_t old_length = table->size;                                                                             \
    wasm_rt_##type##_t* old_data = table->data;                                                                    \
    uint64_t new_length = (uint64_t)table->size + delta;                                                           \
    if ( new_length == 0 ) {                                                                                       \
      return 0;                                                                                                    \
    }                                                                                                              \
    if ( ( new_length < old_length ) || ( new_length > table->max_size ) ) {                                       \
      return (uint32_t)-1;                                                                                         \
    }                                                                                                              \
    void* ptr = aligned_alloc( alignof( wasm_rt_##type##_t ), new_length * sizeof( wasm_rt_##type##_t ) );         \
    table->data = static_cast<wasm_rt_##type##_t*>( ptr );                                                         \
    memcpy( table->data, old_data, old_length * sizeof( wasm_rt_##type##_t ) );                                    \
    table->size = new_length;                                                                                      \
    for ( uint32_t i = old_length; i < new_length; i++ ) {                                                         \
      table->data[i] = init;                                                                                       \
    }                                                                                                              \
    free( old_data );                                                                                              \
    return old_length;                                                                                             \
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
