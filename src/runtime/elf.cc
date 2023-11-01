#include <iostream>
#include <math.h>

#include <span>

#include "elf.hh"
#include "runtimestorage.hh"
#include "timer.hh"

using namespace std;

ElfFile::ElfFile( const std::span<const char> file )
{
  base = file.data();
  // Check ELF magic bytes
  CHECK_GE( file.size(), sizeof( Elf64_Phdr ) );
  std::vector<uint8_t> magic( file.begin(), file.begin() + 4 );
  std::vector<uint8_t> expected_magic = { 0x7f, 'E', 'L', 'F' };
  CHECK( magic == expected_magic );

  // Step 0: Read Elf header
  const Elf64_Ehdr* header = reinterpret_cast<const Elf64_Ehdr*>( file.data() );

  // Step 1: Read program headers (should be empty for .o files)
  CHECK_LE( header->e_phoff + header->e_phnum * sizeof( Elf64_Phdr ), file.size() );
  std::span<const Elf64_Phdr> phdrs = { reinterpret_cast<const Elf64_Phdr*>( file.data() + header->e_phoff ),
                                        static_cast<size_t>( header->e_phnum ) };

  for ( const auto& phdr : phdrs ) {
    VLOG( 2 ) << "Type: " << phdr.p_type;
    VLOG( 2 ) << "Offset: " << phdr.p_offset;
    VLOG( 2 ) << "Flags: " << phdr.p_flags;
    VLOG( 2 ) << "Vaddr: " << phdr.p_vaddr;
    VLOG( 2 ) << "File Size: " << phdr.p_filesz;
    VLOG( 2 ) << "Memory Size: " << phdr.p_memsz;
    if ( phdr.p_type == PT_LOAD and phdr.p_offset != 0 ) {
      if ( phdr.p_vaddr != phdr.p_offset ) {
        LOG( ERROR ) << "ELF file contains non-identity mapped segment.";
      }
    }
  }

  // Step 2: Read section headers
  CHECK_LE( header->e_shoff + header->e_shnum * sizeof( Elf64_Shdr ), file.size() );
  std::span<const Elf64_Shdr> sheader = { reinterpret_cast<const Elf64_Shdr*>( file.data() + header->e_shoff ),
                                          static_cast<size_t>( header->e_shnum ) };

  std::span<const char> namestrs { file.data() + sheader[header->e_shstrndx].sh_offset,
                                   sheader[header->e_shstrndx].sh_size };

  size_t program_size = 0;
  for ( size_t i = 0; i < sheader.size(); i++ ) {
    const auto& section = sheader[i];
    // Skip empty sections
    if ( section.sh_size == 0 )
      continue;

    CHECK( !( section.sh_flags & SHF_WRITE ) );
    auto section_name = string_view( namestrs.data() + section.sh_name );
    cerr << "Section " << section_name << " at " << hex << section.sh_addr << endl;
    CHECK_NE( section_name, ".data" );
    CHECK_NE( section_name, ".bss" );

    // Handle sections with content (.text and .rodata and .bss)
    if ( section.sh_type == SHT_PROGBITS || section.sh_type == SHT_NOBITS ) {
      if ( program_size % section.sh_addralign != 0 ) {
        program_size = ( program_size / section.sh_addralign + 1 ) * section.sh_addralign;
      }
      idx_to_offset[i] = program_size;
      program_size += section.sh_size;
    }

    // Process symbol table
    if ( section.sh_type == SHT_SYMTAB ) {
      // Load symbol table
      std::span<const Elf64_Sym> symtb { reinterpret_cast<const Elf64_Sym*>( file.data() + section.sh_offset ),
                                         section.sh_size / sizeof( Elf64_Sym ) };

      // Load symbol table string
      int symbstrs_idx = section.sh_link;
      std::span<const char> symstrs { file.data() + sheader[symbstrs_idx].sh_offset,
                                      sheader[symbstrs_idx].sh_size };

      for ( const auto& symtb_entry : symtb ) {
        if ( symtb_entry.st_name != 0 ) {
          // Funtion/Variable not defined
          if ( symtb_entry.st_shndx == SHN_UNDEF ) {
            continue;
          }

          string_view name = string_view( symstrs.data() + symtb_entry.st_name );
          /* cerr << "Found function " << name << " at " << hex << symtb_entry.st_value << endl; */
          func_map[name] = symtb_entry.st_value;
        }
      }
    }

    CHECK_NE( section.sh_type, SHT_RELA );
    CHECK_NE( section.sh_type, SHT_REL );
    CHECK_NE( section.sh_type, SHT_DYNAMIC );
  }
  size = program_size;
}

const void* ElfFile::get_function( std::string_view name )
{
  CHECK( func_map.contains( name ) );
  size_t off = func_map.at( name );
  return base + off;
}
