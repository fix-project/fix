#pragma once

#include "instance.hh"
#include "wasm-rt.h"

namespace fixpoint {
/* Initialize module instance*/
void* init_module_instance( size_t instance_size, void* encode_name );

/* Traps if src_handle is not accessible, or not a Tree.
 * Also traps if entry_num is too big.*/
void get_tree_entry( void* module_instance, uint32_t src_ro_handle, uint32_t entry_num, uint32_t target_ro_handle );

/* Traps if handle is inaccessible, if handle does not refer to a Blob */
void attach_blob( void* module_instance, wasm_rt_memory_t* target_memory, uint32_t ro_handle );

void detach_mem( void* module_instance, wasm_rt_memory_t* target_memory, uint32_t rw_handle );

/* Traps if the rw_handle does not refer to an MBlob, or the specified size is larger than the size of the MBlob. */
void freeze_blob( void* module_instance, uint32_t rw_handle, size_t size, uint32_t ro_handle );

/* Traps if ro_hander refers to nothing or is not accessible. */
void designate_output( void* module_instance, uint32_t ro_handle );
}
