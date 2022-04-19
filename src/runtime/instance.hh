#pragma once

#include "absl/container/flat_hash_map.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "mutablevalue.hh"
#include "name.hh"
#include "wasm-rt.h"

class Instance
{
private:
  // Name of the program
  std::string program_name_;

  // Name of encode
  Name encode_name_;

  // Read-only handles
  ObjectReference ro_handle[16];

  // Read-write handles
  MutableValueReference rw_handle[16];

  // Output
  Name output_;

public:
  Instance( std::string program_name, Name encode_name )
    : program_name_( program_name )
    , encode_name_( encode_name )
    , ro_handle()
    , rw_handle()
    , output_()
  {}

  Instance( __m256i encode_name )
    : program_name_( "" )
    , encode_name_( Name( encode_name ) )
    , ro_handle()
    , rw_handle()
    , output_()
  {
    ro_handle[0] = ObjectReference( encode_name_ );
  }

  Instance()
    : program_name_( "" )
    , encode_name_()
    , ro_handle()
    , rw_handle()
    , output_()
  {}

  Instance( const Instance& ) = default;
  Instance& operator=( const Instance& ) = default;

  MutableValueReference* get_rw_handles() { return &rw_handle[0]; }
  ObjectReference* get_ro_handles() { return &ro_handle[0]; }

  void set_output( Name name ) { output_ = name; }
  Name get_output() { return output_; }

  const ObjectReference& get_ro_handle( uint32_t index )
  {
    // need to error check index
    const ObjectReference& ref = ro_handle[index];
    return ref;
  }
};
