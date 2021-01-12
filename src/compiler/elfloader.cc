#include <iostream>

#include "elfloader.hh" 
#include "spans.hh"

using namespace std;

Elf_Info load_program( string & program_content )
{
  Elf_Info res;
  
	// Step 0: Read Elf header
	const Elf64_Ehdr *header = reinterpret_cast<const Elf64_Ehdr *>( program_content.data() );

	// Step 1: Read program headers (should be empty for .o files)
  if ( header->e_phnum != 0 )
  {
    cerr << "Elf64_Phdr should be empty." << endl;
  }

	// Step 2: Read section headers
	res.sheader = span_view<Elf64_Shdr>( reinterpret_cast<const Elf64_Shdr *>( program_content.data() + header->e_shoff ), static_cast<size_t>( header->e_shnum ) );

	// Step 2.1: Read section header string table
	res.namestrs = string_view( program_content.data() + res.sheader[ header->e_shstrndx ].sh_offset, res.sheader[ header->e_shstrndx ].sh_size); 
 
	vector<int> com_symtb_entry;
	map<string, func> func_map;	
	
  int i = 0;
  for ( const auto & section : res.sheader ) {
		printf("S[] sh_offset:%lx sh_name:%s sh_addralign:%lx\n", section.sh_offset, res.namestrs.data() + section.sh_name, section.sh_addralign);
		
		// Allocate code block for .text section
		if ( strcmp( res.namestrs.data() + section.sh_name, ".text" ) != 0 )
    {
			res.text_idx = i;
      res.code = string_view( program_content.data() + section.sh_offset, section.sh_size );
		}

		// Record size of .bss section
		if ( strcmp( res.namestrs.data() + section.sh_name, ".bss" ) != 0 )
    {
			res.bss_size = section.sh_size;
			res.bss_idx = i;
		}

		// Process symbol table
		if( section.sh_type == SHT_SYMTAB ) 
    {
			// Load symbol table
      res.symtb = span_view<Elf64_Sym>( string_view( program_content.data() + section.sh_offset, section.sh_size ) );
			
			// Load symbol table string
			int symbstrs_idx = section.sh_link;
      res.symstrs = string_view( program_content.data() + res.sheader[ symbstrs_idx ].sh_offset, res.sheader[ symbstrs_idx ].sh_size );
		  
      int j = 0;
			for ( const auto & symtb_entry : res.symtb ) 
      {
				if ( symtb_entry.st_name != 0 ) 
        {
					printf("Idx:%d Name:%s value:%lx size:%lx st_shndx:%x\n", j, res.symstrs.data() + symtb_entry.st_name, symtb_entry.st_value, symtb_entry.st_size, symtb_entry.st_shndx);
					// Funtion/Variable not defined
					if ( symtb_entry.st_shndx == SHN_UNDEF ) 
          {
						printf( "Function not defined\n" );
						continue;
					} 

				 	string name = string( res.symstrs.data() + symtb_entry.st_name );

					// A function
					if ( symtb_entry.st_shndx == res.text_idx ) 
          {
            func_map.insert( pair<string, func>( name, func( j, TEXT ) ) );
						printf("Added function:%s at idx%d\n", name.c_str(), j);
					}
					// An .bss variable
					else if ( symtb_entry.st_shndx == res.bss_idx ) 
          {
            func_map.insert( pair<string, func>( name, func( j, BSS ) ) );
						printf("Added bss variable:%s at idx%d\n", name.c_str(), j);
					}
					// An COM variable
					else if ( symtb_entry.st_shndx == SHN_COMMON ) 
          {
            func_map.insert( pair<string, func>( name, func( j, COM ) ) );
						printf("Added *COM* variable:%s at idx%d\n", name.c_str(), j);
						res.com_size += symtb_entry.st_size;
						com_symtb_entry.push_back( j );
					}
				}
        j++;
			}
		}

    
		// Load relocation table
		if ( section.sh_type == SHT_RELA || section.sh_type == SHT_REL ) 
    {
			if( strcmp( res.namestrs.data() + section.sh_name, ".rela.text" ) != 0 )
      {
				res.reloctb = span_view<Elf64_Rela>( string_view( program_content.data() + section.sh_offset, section.sh_size ) ); 
			}
		}


		if( section.sh_type == SHT_DYNAMIC ) 
    {
			cerr << "This is a dynamic linking table.\n";
		}
    i++;
  }  
  
  return res;
}

