#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string>
#include <elf.h>
#include <vector>
#include <sys/mman.h>

// Symbol Type 
#define TEXT 0
#define BSS 1
#define COM 2
#define LIB 3

// Represents one object file
struct Elf_Info {
	// Code section of the object file
	const char *code;
	// BSS section of the object file
	const char *bss;
	
	// String signs for symbol table
	const char *symstrs;
	// String signs for section header 
	const char *namestrs;
	// Symbol table
	const Elf64_Sym *symtb;
	// Number of relocation entries in relocation table
	int reloctb_size;
	// Relocation table
	const Elf64_Rela *reloctb;
	// Section header
	const Elf64_Shdr *sheader;
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

std::string link_program( std::string & program_content, Elf_Info elf_info );
