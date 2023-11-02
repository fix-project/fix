#pragma once
#include <cassert>
#include <cmath>
#include <immintrin.h>

#include "fixpointapi.hh"
#include "wasm-rt.h"

#include <glog/logging.h>

struct api
{
  void ( *attach_blob )( struct w2c_fixpoint*, __m256i, wasm_rt_memory_t* );
  void ( *attach_tree )( struct w2c_fixpoint*, __m256i, wasm_rt_externref_table_t* );
  __m256i ( *create_blob )( struct w2c_fixpoint*, uint32_t, wasm_rt_memory_t* );
  __m256i ( *create_tree )( struct w2c_fixpoint*, uint32_t, wasm_rt_externref_table_t* );
  void ( *unsafely_log )( struct w2c_fixpoint*, uint32_t, uint32_t, wasm_rt_memory_t* );
  __m256i ( *create_blob_i32 )( struct w2c_fixpoint*, uint32_t );
  __m256i ( *create_tag )( struct w2c_fixpoint*, __m256i, __m256i );
  __m256i ( *create_thunk )( struct w2c_fixpoint*, __m256i );
  uint32_t ( *equality )( struct w2c_fixpoint*, __m256i, __m256i );
  __m256i ( *lower )( struct w2c_fixpoint*, __m256i );
  uint32_t ( *get_access )( struct w2c_fixpoint*, __m256i );
  uint32_t ( *get_length )( struct w2c_fixpoint*, __m256i );
  uint32_t ( *get_value_type )( struct w2c_fixpoint*, __m256i );
};

struct rt
{
  void ( *trap )( wasm_rt_trap_t );
  const char* ( *strerror )( wasm_rt_trap_t );
  void ( *load_exception )( const wasm_rt_tag_t, uint32_t, const void* );
  void ( *throw_fn )();
  WASM_RT_UNWIND_TARGET* ( *get_unwind_target )();
  void ( *set_unwind_target )( WASM_RT_UNWIND_TARGET* );
  wasm_rt_tag_t ( *exception_tag )();
  uint32_t ( *exception_size )();
  void* ( *exception )();
  void ( *allocate_memory )( wasm_rt_memory_t*, uint64_t, uint64_t, bool );
  void ( *allocate_memory_sw_checked )( wasm_rt_memory_t*, uint64_t, uint64_t, bool );
  uint64_t ( *grow_memory )( wasm_rt_memory_t*, uint64_t );
  uint64_t ( *grow_memory_sw_checked )( wasm_rt_memory_t*, uint64_t );
  void ( *free_memory )( wasm_rt_memory_t* );
  void ( *free_memory_sw_checked )( wasm_rt_memory_t* );
  void ( *allocate_funcref_table )( wasm_rt_funcref_table_t*, uint32_t, uint32_t );
  void ( *free_funcref_table )( wasm_rt_funcref_table_t* );
  uint32_t ( *grow_funcref_table )( wasm_rt_funcref_table_t*, uint32_t, wasm_rt_funcref_t );
  void ( *allocate_externref_table )( wasm_rt_externref_table_t*, uint32_t, uint32_t );
  void ( *free_externref_table )( wasm_rt_externref_table_t* );
  uint32_t ( *grow_externref_table )( wasm_rt_externref_table_t*, uint32_t, wasm_rt_externref_t );
};

struct os
{
  void* ( *aligned_alloc )( size_t, size_t );
  void ( *free )( void* );
  void ( *__assert_fail )( const char*, const char*, unsigned int, const char* );
  void* ( *memcpy )( void*, void const*, size_t );
  void* ( *memmove )( void*, void const*, size_t );
  void* ( *memset )( void*, int, size_t );
  double ( *sqrt )( double );
  float ( *ceilf )( float );
  double ( *ceil )( double );
  double ( *floor )( double );
  double ( *nearbyint )( double );
};

struct w2c_fixpoint
{
  void* runtime;
  struct api* api;
  struct os* os;
  struct rt* rt;
};

static void fixpoint_assert_fail( const char* assertion, const char* file, unsigned int line, const char* function )
{
  (void)assertion;
  (void)file;
  (void)line;
  (void)function;
  LOG( ERROR ) << "Assertion failed in Fix code.";
  // TODO: these pointers are actually offsets into the binary, so we need the base address to be able to
  // dereference them
  abort();
}

class Runtime;
class FixpointApi
{
  api api_ = {
    &fixpoint::attach_blob,
    &fixpoint::attach_tree,
    &fixpoint::create_blob,
    &fixpoint::create_tree,
    &fixpoint::unsafely_log,
    &fixpoint::create_blob_i32,
    &fixpoint::create_tag,
    &fixpoint::create_thunk,
    &fixpoint::equality,
    &fixpoint::lower,
    &fixpoint::get_access,
    &fixpoint::get_length,
    &fixpoint::get_value_type,
  };

  os os_ = {
    &aligned_alloc,
    &free,
    &fixpoint_assert_fail,
    &memcpy,
    &memmove,
    &memset,
    &sqrt,
    &ceilf,
    &ceil,
    &floor,
    &nearbyint,
  };

  rt rt_ = {
    &wasm_rt_trap,
    &wasm_rt_strerror,
    &wasm_rt_load_exception,
    &wasm_rt_throw,
    &wasm_rt_get_unwind_target,
    &wasm_rt_set_unwind_target,
    &wasm_rt_exception_tag,
    &wasm_rt_exception_size,
    &wasm_rt_exception,
    &wasm_rt_allocate_memory,
    &wasm_rt_allocate_memory_sw_checked,
    &wasm_rt_grow_memory,
    &wasm_rt_grow_memory_sw_checked,
    &wasm_rt_free_memory,
    &wasm_rt_free_memory_sw_checked,
    &wasm_rt_allocate_funcref_table,
    &wasm_rt_free_funcref_table,
    &wasm_rt_grow_funcref_table,
    &wasm_rt_allocate_externref_table,
    &wasm_rt_free_externref_table,
    &wasm_rt_grow_externref_table,
  };

  struct w2c_fixpoint ctx_;

public:
  FixpointApi( Runtime& runtime )
    : ctx_ { &runtime, &api_, &os_, &rt_ }
  {}

  struct w2c_fixpoint* get_context() { return &ctx_; }
};
