#include "elfloader.hh" 

using namespace std;

Elf_Info load_program( string & program_content )
{
  Elf_Info res;
  res.code = program_content.data();
  return res;
}

string link_program( string & program_content, Elf_Info elf_info )
{
  for ( int i = 0; i < elf_info.reloctb_size; i++ ) {}

  return program_content;
}
