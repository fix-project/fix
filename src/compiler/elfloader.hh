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

// A function in a program
struct symbol
{
  // Idx of the symbol in the program's symbol table
  uint64_t idx_;
  // Section that the symbol comes from
  uint64_t section_idx_;
  // Address of the function if it's a library function
  uint64_t lib_addr_;
  // Whether this symbol is lib or not
  bool is_lib;

  symbol( uint64_t idx, uint64_t section_idx )
    : idx_( idx )
    , section_idx_( section_idx )
    , lib_addr_( 0 )
    , is_lib( false )
  {
  }

  symbol( uint64_t lib_addr )
    : idx_( 0 )
    , section_idx_( 0 )
    , lib_addr_( lib_addr )
    , is_lib( true )
  {
  }

  symbol()
    : idx_( 0 )
    , section_idx_( 0 )
    , lib_addr_( 0 )
    , is_lib( false )
  {
  }
};

// Represents one object file
struct Elf_Info
{
  // Size of the whold program
  size_t size;

  // String signs for symbol table
  std::string_view symstrs;
  // String signs for section header
  std::string_view namestrs;
  // Symbol table
  span_view<Elf64_Sym> symtb;
  // Section header
  span_view<Elf64_Shdr> sheader;

  // Map from function/variable name to func entry
  std::map<std::string, symbol> func_map;

  // Map from section name to the offset of section in program memory
  // std::map<std::string, uint64_t> section_to_offset;
  // Map from section idx to the offset of section in program memory
  std::map<uint64_t, uint64_t> idx_to_offset;
  // Map from section idx to section name
  // std::map<uint64_t, std::string> idx_to_section_name;
  // Map from section idx to section content
  // std::map<uint64_t, std::string_view> idx_to_section_content;
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

Elf_Info load_program( std::string& program_content );
Program link_program( std::string& program_content, const std::string& program_name );
