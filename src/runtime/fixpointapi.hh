#pragma once

#include "wasm-rt.h"

struct w2c_fixpoint;
namespace fixpoint {
void attach_tree( w2c_fixpoint* ctx, __m256i ro_handle, wasm_rt_externref_table_t* target_memory );

/* Traps if handle is inaccessible, if handle does not refer to a Blob */
void attach_blob( w2c_fixpoint* ctx, __m256i ro_handle, wasm_rt_memory_t* target_memory );

__m256i create_tree( w2c_fixpoint* ctx, uint32_t size, wasm_rt_externref_table_t* table );

__m256i create_blob( w2c_fixpoint* ctx, uint32_t size, wasm_rt_memory_t* memory );

__m256i create_tag( w2c_fixpoint* ctx, __m256i handle, __m256i type );

__m256i create_blob_i32( w2c_fixpoint* ctx, uint32_t content );

__m256i create_thunk( w2c_fixpoint* ctx, __m256i ro_handle );

uint32_t get_value_type( w2c_fixpoint* ctx, __m256i handle );

// unsafely_log prints the contents mem[str_index: index + length]. Traps if
// index + length - 1 is out of bounds.
void unsafely_log( w2c_fixpoint* ctx, uint32_t index, uint32_t length, wasm_rt_memory_t* mem );

uint32_t equality( w2c_fixpoint* ctx, __m256i lhs, __m256i rhs );

// Length in elements ( w2c_fixpoint *ctx,Tree-like) or bytes (Blobs)
uint32_t get_length( w2c_fixpoint* ctx, __m256i handle );

// Accessibility ( w2c_fixpoint *ctx,0=strict, 1=shallow, 2=lazy)
uint32_t get_access( w2c_fixpoint* ctx, __m256i handle );

// Reduce the accessibility of a Handle
__m256i lower( w2c_fixpoint* ctx, __m256i handle );

}

namespace fixpoint_debug {
// Attempt to lift the current handle to a stronger accessibility ( w2c_fixpoint *ctx,nondeterministic)
__m256i try_lift( w2c_fixpoint* ctx, __m256i handle );

// Attempt to get the Encode from a Handle to a Thunk ( w2c_fixpoint *ctx,nondeterministic if we add GC)
__m256i try_inspect( w2c_fixpoint* ctx, __m256i handle );

// Attempt to get the evaluated version of a Handle ( w2c_fixpoint *ctx,nondeterministic)
__m256i try_evaluate( w2c_fixpoint* ctx, __m256i handle );
}
