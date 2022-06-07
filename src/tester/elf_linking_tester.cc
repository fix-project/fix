#include <fstream>
#include <iostream>
#include <string>

#include "elfloader.hh"
#include "mmap.hh"

using namespace std;

int main( int argc, char* argv[] )
{
  if ( argc != 3 ) {
    cerr << "Usage: " << argv[0] << " input_path output_path\n";
  }

  ReadOnlyFile elf_content { argv[1] };

  auto elf_info = load_program( elf_content );

  ofstream fout( argv[2] );
  fout << string_view( elf_content );
  fout.close();

  for ( auto const& pair : elf_info.func_map ) {
    cout << pair.first << endl;
  }

  link_program( elf_content );

  return 0;
}
