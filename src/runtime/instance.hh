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

public:
  Instance( std::string program_name, Name encode_name )
    : program_name_( program_name )
    , encode_name_( encode_name )
    , ro_handle()
    , rw_handle()
  {}

  Instance()
    : program_name_( "" )
    , encode_name_()
    , ro_handle()
    , rw_handle()
  {}

  Instance( const Instance& ) = default;
  Instance& operator=( const Instance& ) = default;
};
