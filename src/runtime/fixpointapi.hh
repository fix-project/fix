#pragma once

#include "wasm-rt.h"

namespace fixpoint {
void attach_tree( __m256i ro_handle, wasm_rt_externref_table_t* target_memory );

/* Traps if handle is inaccessible, if handle does not refer to a Blob */
void attach_blob( __m256i ro_handle, wasm_rt_memory_t* target_memory );

__m256i detach_mem( wasm_rt_memory_t* target_memory );

__m256i detach_table( wasm_rt_externref_table_t* target_table );

/* Traps if the rw_handle does not refer to an MBlob, or the specified size is larger than the size of the MBlob. */
__m256i freeze_blob( __m256i rw_handle, size_t size );

__m256i freeze_tree( __m256i rw_handle, size_t size );
}
