#pragma once

#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "program.hh"

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
  std::span<const Elf64_Sym> symtb;
  // Section header
  std::span<const Elf64_Shdr> sheader;

  // Map from function/variable name to st_value
  std::map<std::string, std::pair<uint64_t, unsigned short>> func_map;

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
  {}
};

Elf_Info load_program( std::span<const char> program_content );
std::shared_ptr<Program> link_program( std::span<const char> program_content );
