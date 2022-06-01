#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "elfloader.hh"
#include "util.hh"

using namespace std;

int main( int argc, char* argv[] )
{
  if ( argc != 3 ) {
    cerr << "Usage: " << argv[0] << " input_path output_path\n";
  }

  string elf_content = util::read_file( argv[1] );
  auto elf_info = load_program( elf_content );

  ofstream fout( argv[2] );
  fout << elf_content;
  fout.close();

  for ( auto const& pair : elf_info.func_map ) {
    cout << pair.first << endl;
  }

  string program_name( basename( argv[1] ) );
  link_program( elf_content, program_name );

  return 0;
}
