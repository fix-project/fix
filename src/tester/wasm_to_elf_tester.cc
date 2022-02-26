#include <fstream>
#include <iostream>
#include <string>

#include "ccompiler.hh"
#include "util.hh"
#include "wasmcompiler.hh"
#include "initcomposer.hh"

using namespace std;

int main( int argc, char* argv[] )
{
  if ( argc != 5 ) {
    cerr << "Usage: " << argv[0] << " wasm_name path_to_wasm_file\n";
  }

  string wasm_content = util::read_file( argv[2] );
  auto [c_header, h_header] = wasmcompiler::wasm_to_c( argv[1], wasm_content );
  auto init = initcomposer::compose_init( argv[1] );
  string wasm_rt_content = util::read_file( argv[3] );
  string elf_res = c_to_elf( argv[1], c_header, h_header, wasm_rt_content );

  return 0;
}
