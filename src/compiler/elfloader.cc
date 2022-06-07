#include <iostream>

#include "elfloader.hh"
#include "spans.hh"

using namespace std;

static void throw_assertion_failure( const char* __assertion,
                                     const char* __file,
                                     unsigned int __line,
                                     const char* __function )
{
  throw runtime_error( "assertion `" + string( __assertion ) + "' failed at " + string( __file ) + ":"
                       + to_string( __line ) + " (" + string( __function ) + ")" );
}

const static map<string, uint64_t> library_func_map
  = { { "wasm_rt_trap", (uint64_t)wasm_rt_trap },
      { "wasm_rt_register_func_type", (uint64_t)wasm_rt_register_func_type },
      { "wasm_rt_allocate_memory", (uint64_t)wasm_rt_allocate_memory },
      { "wasm_rt_allocate_memory_sw_checked", (uint64_t)wasm_rt_allocate_memory_sw_checked },
      { "wasm_rt_grow_memory", (uint64_t)wasm_rt_grow_memory },
      { "wasm_rt_grow_memory_sw_checked", (uint64_t)wasm_rt_grow_memory_sw_checked },
      { "wasm_rt_allocate_funcref_table", (uint64_t)wasm_rt_allocate_funcref_table },
      { "wasm_rt_allocate_externref_table", (uint64_t)wasm_rt_allocate_externref_table },
      { "wasm_rt_free_memory", (uint64_t)wasm_rt_free_memory },
      { "wasm_rt_free_memory_sw_checked", (uint64_t)wasm_rt_free_memory_sw_checked },
      { "wasm_rt_free_funcref_table", (uint64_t)wasm_rt_free_funcref_table },
      { "wasm_rt_free_externref_table", (uint64_t)wasm_rt_free_externref_table },
      { "wasm_rt_init", (uint64_t)wasm_rt_init },
      { "wasm_rt_free", (uint64_t)wasm_rt_free },
      { "wasm_rt_is_initialized", (uint64_t)wasm_rt_is_initialized },
      { "wasm_rt_register_tag", (uint64_t)wasm_rt_register_tag },
      { "wasm_rt_load_exception", (uint64_t)wasm_rt_load_exception },
      { "wasm_rt_throw", (uint64_t)wasm_rt_throw },
      { "wasm_rt_push_unwind_target", (uint64_t)wasm_rt_push_unwind_target },
      { "wasm_rt_pop_unwind_target", (uint64_t)wasm_rt_pop_unwind_target },
      { "wasm_rt_exception_tag", (uint64_t)wasm_rt_exception_tag },
      { "wasm_rt_exception_size", (uint64_t)wasm_rt_exception_size },
      { "wasm_rt_exception", (uint64_t)wasm_rt_exception },
      { "__sigsetjmp", (uint64_t)__sigsetjmp },
      { "siglongjmp", (uint64_t)siglongjmp },
      { "fixpoint_attach_tree", (uint64_t)fixpoint::attach_tree },
      { "fixpoint_attach_blob", (uint64_t)fixpoint::attach_blob },
      { "fixpoint_create_tree", (uint64_t)fixpoint::create_tree },
      { "fixpoint_create_blob", (uint64_t)fixpoint::create_blob },
      { "fixpoint_create_thunk", (uint64_t)fixpoint::create_thunk },
      { "memcpy", (uint64_t)memcpy },
      { "memmove", (uint64_t)memmove },
      { "__assert_fail", (uint64_t)throw_assertion_failure } };

void __stack_chk_fail( void )
{
  cerr << "stack smashing detected." << endl;
}

Elf_Info load_program( const string_view program_content )
{
  Elf_Info res;

  // Step 0: Read Elf header
  const Elf64_Ehdr* header = reinterpret_cast<const Elf64_Ehdr*>( program_content.data() );

  // Step 1: Read program headers (should be empty for .o files)
  if ( header->e_phnum != 0 ) {
    cerr << "Elf64_Phdr should be empty." << endl;
  }

  // Step 2: Read section headers
  res.sheader
    = span_view<Elf64_Shdr>( reinterpret_cast<const Elf64_Shdr*>( program_content.data() + header->e_shoff ),
                             static_cast<size_t>( header->e_shnum ) );

  // Step 2.1: Read section header string table
  res.namestrs = string_view( program_content.data() + res.sheader[header->e_shstrndx].sh_offset,
                              res.sheader[header->e_shstrndx].sh_size );

  size_t program_size = 0;
  for ( size_t i = 0; i < res.sheader.size(); i++ ) {
    const auto& section = res.sheader[i];
    // Skip empty sections
    if ( section.sh_size == 0 )
      continue;

    // Handle sections with content (.text and .rodata and .bss)
    if ( section.sh_type == SHT_PROGBITS || section.sh_type == SHT_NOBITS ) {
      if ( program_size % section.sh_addralign != 0 ) {
        program_size = ( program_size / section.sh_addralign + 1 ) * section.sh_addralign;
      }
      res.idx_to_offset[i] = program_size;
      program_size += section.sh_size;
    }

    // Process symbol table
    if ( section.sh_type == SHT_SYMTAB ) {
      // Load symbol table
      res.symtb
        = span_view<Elf64_Sym>( string_view( program_content.data() + section.sh_offset, section.sh_size ) );

      // Load symbol table string
      int symbstrs_idx = section.sh_link;
      res.symstrs = string_view( program_content.data() + res.sheader[symbstrs_idx].sh_offset,
                                 res.sheader[symbstrs_idx].sh_size );

      for ( const auto& symtb_entry : res.symtb ) {
        if ( symtb_entry.st_name != 0 ) {
          // Funtion/Variable not defined
          if ( symtb_entry.st_shndx == SHN_UNDEF ) {
            continue;
          }

          string name = string( res.symstrs.data() + symtb_entry.st_name );
          res.func_map[name] = symtb_entry.st_value;
        }
      }
    }

    // Load relocation table
    if ( section.sh_type == SHT_RELA || section.sh_type == SHT_REL ) {
      res.relocation_tables.push_back( i );
    }

    if ( section.sh_type == SHT_DYNAMIC ) {
      cerr << "This is a dynamic linking table.\n";
    }
  }
  res.size = program_size;
  return res;
}

Program link_program( const string_view program_content, const string& program_name )
{
  Elf_Info elf_info = load_program( program_content );

  // Step 0: allocate memory for data and text
  void* program_mem = 0;
  if ( posix_memalign( &program_mem, getpagesize(), elf_info.size ) ) {
    cerr << "Failed to allocate memory.\n";
  }
  if ( mprotect( program_mem, elf_info.size, PROT_EXEC | PROT_READ | PROT_WRITE ) ) {
    cerr << "Failed to set code buffer executable.\n";
  }
  memset( program_mem, 0, elf_info.size );
  // Step 1: Copy sections with initialization data
  for ( const auto& [section_idx, section_offset] : elf_info.idx_to_offset ) {
    const auto& section = elf_info.sheader[section_idx];
    // Sections with initialization data
    if ( section.sh_type == SHT_PROGBITS ) {
      memcpy( static_cast<char*>( program_mem ) + section_offset,
              program_content.data() + section.sh_offset,
              section.sh_size );
    }
  }

  // Step 3: Relocate every section
  for ( const auto& reloc_table_idx : elf_info.relocation_tables ) {
    const auto& section = elf_info.sheader[reloc_table_idx];
    span_view<Elf64_Rela> reloctb
      = span_view<Elf64_Rela>( string_view( program_content.data() + section.sh_offset, section.sh_size ) );

    if ( elf_info.idx_to_offset.find( section.sh_info ) != elf_info.idx_to_offset.end() ) {
      for ( const auto& reloc_entry : reloctb ) {
        int idx = ELF64_R_SYM( reloc_entry.r_info );

        int64_t rel_offset;
        if ( ELF64_R_TYPE( reloc_entry.r_info ) == R_X86_64_PC32
             || ELF64_R_TYPE( reloc_entry.r_info ) == R_X86_64_PC64
             || ELF64_R_TYPE( reloc_entry.r_info ) == R_X86_64_PLT32 ) {
          // S + A - P or L + A - P
          rel_offset
            = reloc_entry.r_addend
              - ( (int64_t)program_mem + elf_info.idx_to_offset.at( section.sh_info ) + reloc_entry.r_offset );
        } else if ( ELF64_R_TYPE( reloc_entry.r_info ) == R_X86_64_64
                    || ELF64_R_TYPE( reloc_entry.r_info ) == R_X86_64_32 ) {
          // S + A
          rel_offset = reloc_entry.r_addend;
        } else {
          throw out_of_range( "Relocation type not supported." );
        }

        const auto& symtb_entry = elf_info.symtb[idx];
        // Handle relocation for section
        if ( ELF64_ST_TYPE( symtb_entry.st_info ) == STT_SECTION ) {
          rel_offset += (int64_t)( program_mem ) + elf_info.idx_to_offset.at( symtb_entry.st_shndx );
        }
        // Handle relcoation for function and data
        else {
          if ( symtb_entry.st_shndx == SHN_UNDEF ) {
            string name = string( elf_info.symstrs.data() + symtb_entry.st_name );
            if ( library_func_map.count( name ) != 1 ) {
              throw runtime_error( "can't find function " + name );
            }
            rel_offset += library_func_map.at( name );
          } else {
            rel_offset += (int64_t)( program_mem ) + elf_info.idx_to_offset.at( symtb_entry.st_shndx )
                          + symtb_entry.st_value;
          }
        }

        // qword relocation
        if ( ELF64_R_TYPE( reloc_entry.r_info ) == R_X86_64_64
             || ELF64_R_TYPE( reloc_entry.r_info ) == R_X86_64_PC64 ) {
          memcpy( static_cast<char*>( program_mem ) + elf_info.idx_to_offset.at( section.sh_info )
                    + reloc_entry.r_offset,
                  &rel_offset,
                  sizeof( int64_t ) );
        } else {
          int32_t rel_offset_32 = (int32_t)rel_offset;
          memcpy( static_cast<char*>( program_mem ) + elf_info.idx_to_offset.at( section.sh_info )
                    + reloc_entry.r_offset,
                  &rel_offset_32,
                  sizeof( int32_t ) );
        }
      }
    }
  }

  shared_ptr<char> code( static_cast<char*>( program_mem ), free );
  uint64_t init_entry = elf_info.func_map.at( "initProgram" );
  uint64_t main_entry = elf_info.func_map.at( "Z_" + program_name + "Z__fixpoint_apply" );
  uint64_t cleanup_entry = elf_info.func_map.at( "Z_" + program_name + "_free" );
  uint64_t instance_size_entry = elf_info.func_map.at( "get_instance_size" );
  uint64_t init_module_entry = elf_info.func_map.at( "Z_" + program_name + "_init_module" );
  return Program(
    program_name, code, init_entry, main_entry, cleanup_entry, instance_size_entry, init_module_entry );
}
