#include <iostream>

#include "timing_helper.hh"
#include "wasienv.hh"
#include "runtimestorage.hh"

using namespace std;

namespace wasi
{
  void attach_input( uint32_t input_index, uint32_t mem_index )
  {
    auto & invocation = id_to_inv_.at( invocation_id_ );
    invocation.attach_input( input_index, mem_index );
  }

  void attach_output( uint32_t output_index, uint32_t mem_index, uint32_t output_type )
  {
    auto & invocation = id_to_inv_.at( invocation_id_ );
    invocation.attach_output( output_index, mem_index, output_type );
  }
   
  uint32_t get_int( uint32_t mem_index, uint32_t ofst )
  {
    auto & invocation = id_to_inv_.at( invocation_id_ );
    return invocation.get_int( mem_index, ofst );
  }

  void store_int( uint32_t mem_index, uint32_t ofst )
  {
    auto & invocation = id_to_inv_.at( invocation_id_ );
    invocation.store_int( mem_index, ofst );
  }

  void attach_output_child ( uint32_t parent_mem_index, uint32_t child_index, uint32_t child_mem_index, uint32_t output_type )
  {
    auto & invocation = id_to_inv_.at( invocation_id_ );
    invocation.attach_output_child ( parent_mem_index, child_index, child_mem_index, output_type );
  }

  void set_encode( uint32_t mem_index, uint32_t encode_index )
  {
    auto & invocation = id_to_inv_.at( invocation_id_ );
    invocation.set_encode( mem_index, encode_index );
  }

  void add_path( uint32_t mem_index, uint32_t path )
  {
    auto & invocation = id_to_inv_.at( invocation_id_ );
    invocation.add_path( mem_index, path );
  }

  void move_lazy_input( uint32_t mem_index, uint32_t child_index, uint32_t lazy_input_index )
  {
    auto & invocation = id_to_inv_.at( invocation_id_ );
    invocation.move_lazy_input( mem_index, child_index, lazy_input_index );
  }
}
