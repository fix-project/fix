#include <fstream>
#include <iostream>
#include <string>

#include "ccompiler.hh"
#include "util.hh"

using namespace std;

int main( int argc, char* argv[] )
{
  if ( argc != 6 ) {
    cerr << "Usage: " << argv[0] << "name path_to_c path_to_h path_to_wasm_rt output_directory\n";
    return 1;
  }

  string c_header = util::read_file( argv[2] );
  string h_header = util::read_file( argv[3] );

  string wasm_rt_content = util::read_file( argv[4] );
  string elf_res = c_to_elf( argv[1], c_header, h_header, wasm_rt_content );

  // cout << elf_res << endl;
  ofstream fout( argv[5] );
  fout << elf_res;
  fout.close();

  return 0;
}
