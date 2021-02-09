#include <iostream>

#include "elfloader.hh" 
#include "spans.hh"


using namespace std;

void __stack_chk_fail(void) {
  cerr << "stack smashing detected." << endl;
}

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
		
		// Allocate code block for .text section
		if ( strcmp( res.namestrs.data() + section.sh_name, ".text" ) == 0 )
    {
      res.text_idx = i;
      res.code = string_view( program_content.data() + section.sh_offset, section.sh_size );
		}

		// Record size of .bss section
		if ( strcmp( res.namestrs.data() + section.sh_name, ".bss" ) == 0 )
    {
			res.bss_size = section.sh_size;
			res.bss_idx = i;
		}

		if ( strcmp( res.namestrs.data() + section.sh_name, ".rodata" ) == 0 || strcmp( res.namestrs.data() + section.sh_name, ".rodata.str1.1" ) == 0 )
    {
      res.rodata_idx = i;
      res.rodata = string_view( program_content.data() + section.sh_offset, section.sh_size );
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
					// Funtion/Variable not defined
					if ( symtb_entry.st_shndx == SHN_UNDEF ) 
          {
            j++;
						continue;
					} 

				 	string name = string( res.symstrs.data() + symtb_entry.st_name );

					// A function
					if ( symtb_entry.st_shndx == res.text_idx ) 
          {
            res.func_map.insert( pair<string, func>( name, func( j, TEXT ) ) );
					}
					// An .bss variable
					else if ( symtb_entry.st_shndx == res.bss_idx ) 
          {
            res.func_map.insert( pair<string, func>( name, func( j, BSS ) ) );
					}
					// An COM variable
					else if ( symtb_entry.st_shndx == SHN_COMMON ) 
          {
            res.func_map.insert( pair<string, func>( name, func( j, COM ) ) );
						res.com_size += symtb_entry.st_size;
						res.com_symtb_entry.push_back( j );
					}
          else if ( symtb_entry.st_shndx == res.rodata_idx )
          {
            res.func_map.insert( pair<string, func>( name, func( j, RODATA ) ) );
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
	uint64_t com_base = res.rodata.size() + res.bss_size;
	for ( int com_sym_idx : res.com_symtb_entry) 
  {
    Elf64_Sym & mutable_symtb_entry = const_cast<Elf64_Sym &>( res.symtb[com_sym_idx] );
		mutable_symtb_entry.st_value = com_base;
		com_base += mutable_symtb_entry.st_size;
	}

  return res;
}

Program link_program( Elf_Info & elf_info, string & program_name, vector<string> && inputs, vector<string> && outputs )
{
  static Elf64_Addr global_offset_table [4] = 
  {
    reinterpret_cast<Elf64_Addr>( &wasm_rt_call_stack_depth ),
    reinterpret_cast<Elf64_Addr>( &wasi::Z_envZ_path_openZ_ii ),
    reinterpret_cast<Elf64_Addr>( &wasi::Z_envZ_fd_readZ_iiii ),
    reinterpret_cast<Elf64_Addr>( &wasi::Z_envZ_fd_writeZ_iiii ) 
  };
	
  // Step 0: allocate memory for data and text
  size_t mem_size = elf_info.code.size() + elf_info.rodata.size() + elf_info.bss_size + elf_info.com_size;
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
  memcpy( (char *)program_mem + elf_info.code.size(), elf_info.rodata.data(), elf_info.rodata.size() );

	// Step 1: Add wasm-rt functions to func_map	
	elf_info.func_map.insert( pair<string, func>( "wasm_rt_trap", func( (uint64_t)wasm_rt_trap ) ) );		
	elf_info.func_map.insert( pair<string, func>( "wasm_rt_register_func_type", func( (uint64_t)wasm_rt_register_func_type ) ) );		
	elf_info.func_map.insert( pair<string, func>( "wasm_rt_allocate_memory", func( (uint64_t)wasm_rt_allocate_memory ) ) );		
	elf_info.func_map.insert( pair<string, func>( "wasm_rt_grow_memory", func( (uint64_t)wasm_rt_grow_memory ) ) );		
	elf_info.func_map.insert( pair<string, func>( "wasm_rt_allocate_table", func( (uint64_t)wasm_rt_allocate_table ) ) );		
	elf_info.func_map.insert( pair<string, func>( "wasm_rt_call_stack_depth", func( (uint64_t)&wasm_rt_call_stack_depth ) ) );		
	elf_info.func_map.insert( pair<string, func>( "Z_envZ_path_openZ_ii", func( (uint64_t)&wasi::Z_envZ_path_openZ_ii ) ) );		
	elf_info.func_map.insert( pair<string, func>( "Z_envZ_fd_readZ_iiii", func( (uint64_t)&wasi::Z_envZ_fd_readZ_iiii ) ) );		
	elf_info.func_map.insert( pair<string, func>( "Z_envZ_fd_writeZ_iiii", func( (uint64_t)&wasi::Z_envZ_fd_writeZ_iiii ) ) );		
  elf_info.func_map.insert( pair<string, func>( "__stack_chk_fail", func( (uint64_t)__stack_chk_fail ) ) );

  for (const auto & reloc_entry : elf_info.reloctb ){
    int idx = ELF64_R_SYM( reloc_entry.r_info );

    int32_t rel_offset;
    if ( ELF64_R_TYPE( reloc_entry.r_info ) == 2 || ELF64_R_TYPE( reloc_entry.r_info ) == 4 || ELF64_R_TYPE( reloc_entry.r_info ) == 9 )
    {
      rel_offset = reloc_entry.r_addend - (int64_t)program_mem - reloc_entry.r_offset;
	  } else if ( ELF64_R_TYPE( reloc_entry.r_info ) == 10 || ELF64_R_TYPE( reloc_entry.r_info ) == 11 ) 
    {
      rel_offset = reloc_entry.r_addend;
    } else {
      throw out_of_range ( "Relocation type not supported." );
    }
      // Check whether is a section
    if ( ELF64_ST_TYPE( elf_info.symtb[idx].st_info ) == STT_SECTION) 
    {
      string sec_name = string( elf_info.namestrs.data() + elf_info.sheader[elf_info.symtb[idx].st_shndx].sh_name );
      if ( sec_name == ".text" ) 
      {
        rel_offset += (int64_t)(program_mem);
      }
      else if ( sec_name == ".rodata" || sec_name == ".rodata.str1.1" )
      {
        rel_offset += (int64_t)(program_mem) + elf_info.code.size();
      }
      else if ( sec_name == ".bss" ) 
      {
        rel_offset += (int64_t)(program_mem) + elf_info.code.size() + elf_info.rodata.size();
      } 
      else if ( sec_name == ".data" ) 
      {
        printf("No .data section\n");
      }
    } else {		
      string name = string( elf_info.symstrs.data() + elf_info.symtb[idx].st_name );

      if ( ELF64_R_TYPE( reloc_entry.r_info ) == 9 )
      {
        if ( name == "wasm_rt_call_stack_depth" ) 
        {
          rel_offset += (int64_t)&global_offset_table[0];
        } 
        else if ( name == "Z_envZ_path_openZ_ii" ) 
        {
          rel_offset += (int64_t)&global_offset_table[1];
        } 
        else if ( name == "Z_envZ_fd_readZ_iiii" ) 
        {
          rel_offset += (int64_t)&global_offset_table[2];
        } 
        else if ( name == "Z_envZ_fd_writeZ_iiii" ) 
        {
          rel_offset += (int64_t)&global_offset_table[3];
        } 
        else
        {
          throw out_of_range("Global function does not exist.");
        }
      } else {
        func dest = elf_info.func_map.at(name);
        switch ( dest.type ) {
          case LIB:
            rel_offset += (int64_t)(dest.lib_addr);
            break;
          case TEXT:
            rel_offset += (int64_t)(program_mem) + elf_info.symtb[dest.idx].st_value;
            break;	     
          case RODATA:
            rel_offset += (int64_t)(program_mem) + elf_info.code.size() + elf_info.symtb[dest.idx].st_value;
            break;
          case BSS:
          case COM:
            rel_offset += (int64_t)(program_mem) + elf_info.code.size() + elf_info.rodata.size() + elf_info.symtb[dest.idx].st_value;
            break;
        }
      }
    }	
    *((int32_t *)( reinterpret_cast<char *>(program_mem) + reloc_entry.r_offset) ) = rel_offset; 
  }	

  shared_ptr<char> code ( reinterpret_cast<char *>(program_mem) );
  uint64_t init_entry = elf_info.symtb[ elf_info.func_map.at("init").idx ].st_value;
  // uint64_t main_entry = elf_info.symtb[ elf_info.func_map.at("_start").idx ].st_value;
  uint64_t main_entry = elf_info.symtb[ elf_info.func_map.at("w2c__start").idx ].st_value;
  uint64_t mem_loc = elf_info.code.size() + elf_info.rodata.size() + elf_info.symtb[ elf_info.func_map.at("w2c_memory").idx ].st_value;

  return Program( program_name, move(inputs), move(outputs), code, init_entry, main_entry, mem_loc ); 
}
