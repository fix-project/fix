#include <fstream>
#include <iostream>
#include <string>

#include "ccompiler.hh"
#include "initcomposer.hh"
#include "util.hh"
#include "wasmcompiler.hh"

using namespace std;

int main( int argc, char* argv[] )
{
  if ( argc != 5 ) {
    cerr << "Usage: " << argv[0] << " wasm_name path_to_wasm_file  path_to_wasm_rt path_to_obj\n";
  }

  string wasm_content = util::read_file( argv[2] );
  auto [c_header, h_header, fixpoint_header] = wasmcompiler::wasm_to_c( argv[1], wasm_content );
  cout << "Composed c, h, and init" << endl;

  string wasm_rt_content = util::read_file( argv[3] );
  cout << "Read wasm_rt" << endl;

  string elf_res = c_to_elf( argv[1], c_header, h_header, fixpoint_header, wasm_rt_content );

  ofstream fout_obj( argv[4] );
  fout_obj << elf_res;
  fout_obj.close();

  return 0;
}
