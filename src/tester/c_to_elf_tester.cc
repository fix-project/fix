#include <fstream>
#include <iostream>
#include <string>

#include "ccompiler.hh"
#include "mmap.hh"

using namespace std;

int main( int argc, char* argv[] )
{
  if ( argc != 5 ) {
    cerr << "Usage: " << argv[0] << "path_to_c path_to_h path_to_wasm_rt output_directory\n";
    return 1;
  }

  ReadOnlyFile c_header { argv[1] };
  ReadOnlyFile h_header { argv[2] };
  ReadOnlyFile wasm_rt_content { argv[3] };

  string elf_res = c_to_elf( c_header, h_header, "", wasm_rt_content );

  // cout << elf_res << endl;
  ofstream fout( argv[5] );
  fout << elf_res;
  fout.close();

  return 0;
}
