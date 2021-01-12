#include <elf.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "program.hh"
#include "spans.hh"

// Symbol Type 
#define TEXT 0
#define BSS 1
#define COM 2
#define LIB 3

// Represents one object file
struct Elf_Info {
	// Index of the code section
  uint64_t text_idx;
  // Code section 
	std::string_view code;

  // Index of the bss section
  uint64_t bss_idx;
  // Size of the bss section
  uint64_t bss_size;
  // Size of the com section
  uint64_t com_size;

	// String signs for symbol table
	std::string_view symstrs;
	// String signs for section header 
	std::string_view namestrs;
	// Symbol table
	span_view<Elf64_Sym> symtb;
	// Relocation table
	span_view<Elf64_Rela> reloctb;
	// Section header
	span_view<Elf64_Shdr> sheader;

  Elf_Info() 
    : text_idx( 0 ), code(), 
      bss_idx( 0 ), bss_size( 0 ), com_size( 0 ),
      symstrs(), namestrs(), symtb(), reloctb(), sheader() {}
};

// A function in a program
struct func {
	// Idx of the function in the program's symbol table
	uint64_t idx;
	// Type of the function
  int type;
	// Address of the function if it's a library function
	uint64_t lib_addr;

	func( uint64_t idx_, int type_ ) : idx( idx_ ),
                                     type( type_ ),
                                     lib_addr( 0 ) {}

	func( uint64_t lib_addr_ ) : idx( 0 ),
				                       type( LIB ),
                               lib_addr( lib_addr_ ) {}
};

Elf_Info load_progarm( const std::string & program_content );
