#include <iostream>
#include <fstream>
#include <string>

#include "wasmcompiler.hh"
#include "ccompiler.hh"
#include "util.hh"

using namespace std;

int main( int argc, char * argv[] ) 
{ 
  if ( argc != 5 ) 
  {
    cerr << "Usage: " << argv[0] << " wasm_name path_to_wasm_file path_to_wasm_rt path_to_output\n";
  }

  string wasm_content = util::read_file( argv[2] );
  auto [ c_header, h_header ] = wasmcompiler::wasm_to_c( argv[1], wasm_content );

  string wasm_rt_content = util::read_file( argv[3] );
  string elf_res = c_to_elf( argv[1], c_header, h_header, wasm_rt_content );

  // cout << elf_res << endl;
  ofstream fout ( argv[4] );
  fout << elf_res;
  fout.close();

  return 0;
}
     
