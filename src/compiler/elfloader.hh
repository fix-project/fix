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
struct Elf_Info
{
  // Size of the whole program
  size_t size;

  // String signs for symbol table
  std::string_view symstrs;
  // String signs for section header
  std::string_view namestrs;
  // Symbol table
  span_view<Elf64_Sym> symtb;
  // Section header
  span_view<Elf64_Shdr> sheader;

  // Map from function/variable name to st_value
  std::map<std::string, uint64_t> func_map;

  // Map from section idx to the offset of section in program memory
  std::map<uint64_t, uint64_t> idx_to_offset;
  // List of relocation_tables
  std::vector<size_t> relocation_tables;

  Elf_Info()
    : size( 0 )
    , symstrs()
    , namestrs()
    , symtb()
    , sheader()
    , func_map()
    , idx_to_offset()
    , relocation_tables()
  {
  }
};

Elf_Info load_program( const std::string_view program_content );
Program link_program( std::string& program_content, const std::string& program_name );
