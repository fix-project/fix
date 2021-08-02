#pragma once

#include <map>
#include <string>
#include <unordered_map>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "invocation.hh"
#include "wasm-rt.h"

namespace wasi {
inline Invocation invoc_ptr;
inline thread_local wasm_rt_memory_t buf;
inline thread_local wasm_rt_memory_t wasi_buf;

void attach_input( uint32_t input_index, uint32_t mem_index );
void attach_output( uint32_t output_index, uint32_t mem_index, uint32_t output_type );
void attach_output_child( uint32_t parent_mem_index,
                          uint32_t child_index,
                          uint32_t child_mem_index,
                          uint32_t output_type );
void set_encode( uint32_t mem_index, uint32_t encode_index );
void add_path( uint32_t mem_index, uint32_t path );
void move_lazy_input( uint32_t mem_index, uint32_t child_index, uint32_t lazy_input_index );

uint32_t get_int( uint32_t mem_index, uint32_t ofst );
void store_int( uint32_t mem_index, uint32_t ofst );

uint32_t mem_copy( uint32_t mem_index, uint32_t ofst, uint32_t linear_mem_ofst, uint32_t iovs_len );

inline void ( *Z_envZ_attach_inputZ_vii )( uint32_t, uint32_t ) { &attach_input };
inline void ( *Z_envZ_attach_outputZ_viii )( uint32_t, uint32_t, uint32_t ) { &attach_output };
inline void ( *Z_envZ_move_lazy_inputZ_viii )( uint32_t, uint32_t, uint32_t ) { &move_lazy_input };
inline uint32_t ( *Z_envZ_get_intZ_iii )( uint32_t, uint32_t ) { &get_int };
inline void ( *Z_envZ_store_intZ_vii )( uint32_t, uint32_t ) { &store_int };
inline void ( *Z_envZ_set_encodeZ_vii )( uint32_t, uint32_t ) { &set_encode };
inline void ( *Z_envZ_add_pathZ_vii )( uint32_t, uint32_t ) { &add_path };
inline void ( *Z_envZ_attach_output_childZ_viiii )( uint32_t, uint32_t, uint32_t, uint32_t ) {
  &attach_output_child
};

uint32_t wasi_mem_load32( uint32_t offset );
void wasi_mem_store32( uint32_t offset, uint32_t content );
uint8_t wasi_mem_load8( uint32_t offset );
void wasi_mem_store8( uint32_t offset, uint8_t content );
}
