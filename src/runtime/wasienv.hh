#pragma once

#include <map>
#include <unordered_map>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "invocation.hh"
#include "wasm-rt.h"

// ID of the current invocation
namespace wasi 
{
  inline thread_local uint64_t invocation_id_ = -1;
  inline thread_local wasm_rt_memory_t buf;
  inline absl::flat_hash_map<uint64_t, Invocation> id_to_inv_;

  void attach_input( uint32_t input_index, uint32_t mem_index );
  void attach_output( uint32_t output_index, uint32_t mem_index );
   
  uint32_t get_i32( uint32_t mem_index, uint32_t ofst );
  void write_i32( uint32_t mem_index, uint32_t ofst );

  // TODO: tree operations
  void attach_tree_child_input( uint32_t parent_mem_index, uint32_t child_index, uint32_t mem_index );

}
