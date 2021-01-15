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
	
  int i = 0;
  for ( const auto & section : res.sheader ) {
		printf("S[%d] sh_offset:%lx sh_name:%s sh_addralign:%lx\n", i, section.sh_offset, res.namestrs.data() + section.sh_name, section.sh_addralign);
		
		// Allocate code block for .text section
		if ( strcmp( res.namestrs.data() + section.sh_name, ".text" ) == 0 )
    {
			cout << "Assigning text_idx to " << i << endl;
      res.text_idx = i;
      res.code = string_view( program_content.data() + section.sh_offset, section.sh_size );
		}

		// Record size of .bss section
		if ( strcmp( res.namestrs.data() + section.sh_name, ".bss" ) == 0 )
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
            j++;
						continue;
					} 

				 	string name = string( res.symstrs.data() + symtb_entry.st_name );

					// A function
					if ( symtb_entry.st_shndx == res.text_idx ) 
          {
            res.func_map.insert( pair<string, func>( name, func( j, TEXT ) ) );
						printf("Added function:%s at idx%d\n", name.c_str(), j);
					}
					// An .bss variable
					else if ( symtb_entry.st_shndx == res.bss_idx ) 
          {
            res.func_map.insert( pair<string, func>( name, func( j, BSS ) ) );
						printf("Added bss variable:%s at idx%d\n", name.c_str(), j);
					}
					// An COM variable
					else if ( symtb_entry.st_shndx == SHN_COMMON ) 
          {
            res.func_map.insert( pair<string, func>( name, func( j, COM ) ) );
						printf("Added *COM* variable:%s at idx%d\n", name.c_str(), j);
						res.com_size += symtb_entry.st_size;
						res.com_symtb_entry.push_back( j );
					}
				}
        j++;
			}
		}

    
		// Load relocation table
		if ( section.sh_type == SHT_RELA || section.sh_type == SHT_REL ) 
    {
			if( strcmp( res.namestrs.data() + section.sh_name, ".rela.text" ) == 0 )
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
  
	// Step 3: Update symbol table entry for *COM*
	uint64_t com_base = res.bss_size;
	for ( int com_sym_idx : res.com_symtb_entry) 
  {
    cout << "Adjusting com_sym_idx " << com_sym_idx << " to com_base " << com_base << endl;
    Elf64_Sym & mutable_symtb_entry = const_cast<Elf64_Sym &>( res.symtb[com_sym_idx] );
    printf("Idx:%d Name:%s value:%lx size:%lx st_shndx:%x\n", com_sym_idx, res.symstrs.data() + mutable_symtb_entry.st_name, mutable_symtb_entry.st_value, mutable_symtb_entry.st_size, mutable_symtb_entry.st_shndx);
		mutable_symtb_entry.st_value = com_base;
		com_base += mutable_symtb_entry.st_size;
	}

  return res;
}

Program link_program( Elf_Info & elf_info, string & program_name, vector<string> && inputs, vector<string> && outputs )
{
	// Step 0: allocate memory for data and text
  size_t mem_size = elf_info.code.size() + elf_info.bss_size + elf_info.com_size;
  void *program_mem = 0;
  if ( posix_memalign( &program_mem, getpagesize(), mem_size ) )
  {
    cerr << "Failed to allocate memory.\n";
  }
  if ( mprotect( program_mem, mem_size, PROT_EXEC|PROT_READ|PROT_WRITE ) )
  {
    cerr << "Failed to set code buffer executable.\n";
  }
  memcpy( program_mem, elf_info.code.data(), elf_info.code.size() );

	// Step 1: Add wasm-rt functions to func_map	
	elf_info.func_map.insert( pair<string, func>( "wasm_rt_trap", func( (uint64_t)wasm_rt_trap ) ) );		
	elf_info.func_map.insert( pair<string, func>( "wasm_rt_register_func_type", func( (uint64_t)wasm_rt_register_func_type ) ) );		
	elf_info.func_map.insert( pair<string, func>( "wasm_rt_allocate_memory", func( (uint64_t)wasm_rt_allocate_memory ) ) );		
	elf_info.func_map.insert( pair<string, func>( "wasm_rt_grow_memory", func( (uint64_t)wasm_rt_grow_memory ) ) );		
	elf_info.func_map.insert( pair<string, func>( "wasm_rt_allocate_table", func( (uint64_t)wasm_rt_allocate_table ) ) );		
	elf_info.func_map.insert( pair<string, func>( "wasm_rt_call_stack_depth", func( (uint64_t)&wasm_rt_call_stack_depth ) ) );		
	// elf_info.func_map.insert( pair<string, func>( "path_open", func( (uint64_t)path_open ) );		
	// elf_info.func_map.insert( pair<string, func>( "fd_read", func( (uint64_t)fd_read ) );		
	// elf_info.func_map.insert( pair<string, func>( "fd_write", func( (uint64_t)fd_write ) );		
 
  for (const auto & reloc_entry : elf_info.reloctb ){
    printf("offset:%lx index:%lx type:%lx addend:%ld\n", reloc_entry.r_offset, ELF64_R_SYM( reloc_entry.r_info ), ELF64_R_TYPE( reloc_entry.r_info ), reloc_entry.r_addend);
    int idx = ELF64_R_SYM( reloc_entry.r_info );
    int32_t rel_offset = reloc_entry.r_addend - (int64_t)program_mem - reloc_entry.r_offset;
			// Check whether is a section
    if ( ELF64_ST_TYPE( elf_info.symtb[idx].st_info ) == STT_SECTION) 
    {
      string sec_name = string( elf_info.namestrs.data() + elf_info.sheader[elf_info.symtb[idx].st_shndx].sh_name );
      if ( sec_name == ".text" ) 
      {
        rel_offset += (int64_t)(program_mem);
        printf("Reloced .text section\n");
      } 
      else if ( sec_name == ".bss" ) 
      {
        rel_offset += (int64_t)(program_mem) + elf_info.code.size();
        printf("Reloced .bss section\n");
      } 
      else if ( sec_name == ".data" ) 
      {
        printf("No .data section\n");
      }
    } else {		
      string name = string( elf_info.symstrs.data() + elf_info.symtb[idx].st_name );
      printf( "Name is %s\n", name.c_str() );
      func dest = elf_info.func_map.at(name);
      switch ( dest.type ) {
        case LIB:
          rel_offset += (int64_t)(dest.lib_addr);
          break;
        case TEXT:
          rel_offset += (int64_t)(program_mem) + elf_info.symtb[dest.idx].st_value;
          break;	     
        case BSS:
        case COM:
          rel_offset += (int64_t)(program_mem) + elf_info.code.size() + elf_info.symtb[dest.idx].st_value;
          break;
      }
    }	
    *((int32_t *)( reinterpret_cast<char *>(program_mem) + reloc_entry.r_offset) ) = rel_offset; 
    printf( "Value is %d\n", rel_offset );
  }	

  shared_ptr<char> code ( reinterpret_cast<char *>(program_mem) );
  uint64_t init_entry = elf_info.symtb[ elf_info.func_map.at("init").idx ].st_value;
  uint64_t main_entry = elf_info.symtb[ elf_info.func_map.at("_start").idx ].st_value;
  uint64_t mem_loc = elf_info.code.size() + elf_info.symtb[ elf_info.func_map.at("memory").idx ].st_value;

  cout << init_entry << " " << main_entry << endl;
  cout << code.get() << endl;
  return Program( program_name, move(inputs), move(outputs), code, init_entry, main_entry, mem_loc ); 
}
