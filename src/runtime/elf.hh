#pragma once

#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "fixpointapi.hh"
#include "program.hh"
#include "spans.hh"
#include "wasm-rt.h"

// Represents one object file
class ElfFile
{
  // Size of the whole program
  size_t size = 0;
  const char* base = nullptr;

  // Map from function/variable name to st_value
  std::map<std::string_view, uint64_t> func_map {};

  // Map from section idx to the offset of section in program memory
  std::map<uint64_t, uint64_t> idx_to_offset {};

public:
  ElfFile( const std::span<const char> file );
  ElfFile( ElfFile& other )
    : size( other.size )
    , base( other.base )
    , func_map( other.func_map )
    , idx_to_offset( other.idx_to_offset )
  {}
  ElfFile& operator=( ElfFile& other )
  {
    size = other.size;
    base = other.base;
    func_map = other.func_map;
    idx_to_offset = other.idx_to_offset;
    return *this;
  }
  const void* get_function( std::string_view name );
};
