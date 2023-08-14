#pragma once

#include "wasm-rt.h"

namespace fixpoint {
void attach_tree( __m256i ro_handle, wasm_rt_externref_table_t* target_memory );

/* Traps if handle is inaccessible, if handle does not refer to a Blob */
void attach_blob( __m256i ro_handle, wasm_rt_memory_t* target_memory );

__m256i create_tree( wasm_rt_externref_table_t* table, size_t size );

__m256i create_blob( wasm_rt_memory_t* memory, size_t size );

__m256i create_tag( __m256i handle, __m256i type );

__m256i create_blob_i32( uint32_t content );

__m256i create_thunk( __m256i ro_handle );

uint32_t get_value_type( __m256i handle );

// Unsafe_io prints the contents mem[str_index: index + length]. Traps if
// index + length - 1 is out of bounds.
void unsafe_io( int32_t index, int32_t length, wasm_rt_memory_t* mem );

uint32_t equality( __m256i lhs, __m256i rhs );

// Length in elements (Tree-like) or bytes (Blobs)
uint32_t get_length( __m256i handle );

// Accessibility (0=strict, 1=shallow, 2=lazy)
uint32_t get_access( __m256i handle );

// Reduce the accessibility of a Handle
__m256i lower( __m256i handle );

}

namespace fixpoint_debug {
// Attempt to lift the current handle to a stronger accessibility (nondeterministic)
__m256i try_lift( __m256i handle );

// Attempt to get the Encode from a Handle to a Thunk (nondeterministic if we add GC)
__m256i try_inspect( __m256i handle );

// Attempt to get the evaluated version of a Handle (nondeterministic)
__m256i try_evaluate( __m256i handle );
}
