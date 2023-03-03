#pragma once

#include "wasm-rt.h"

namespace fixpoint {
void attach_tree( __m256i ro_handle, wasm_rt_externref_table_t* target_memory );

/* Traps if handle is inaccessible, if handle does not refer to a Blob */
void attach_blob( __m256i ro_handle, wasm_rt_memory_t* target_memory );

__m256i create_tree( wasm_rt_externref_table_t* table, size_t size );

//void copy_input( wasm_rt_externref_table_t* ro_handle, wasm_rt_externref_table_t* rw_handle );
__m256i create_blob( wasm_rt_memory_t* memory, size_t size );

__m256i create_blob_i32( uint32_t content );

__m256i create_thunk( __m256i ro_handle );

uint32_t value_type( __m256i handle );

// Unsafe_io prints the contents mem[str_index: index + length]. Traps if
// index + length - 1 is out of bounds.
void unsafe_io( int32_t index, int32_t length, wasm_rt_memory_t* mem );
}
