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
  if ( argc < 7 ) {
    cerr << "Usage: " << argv[0] << " wasm_name path_to_wasm_file path_to_c path_to_h path_to_init start_func\n";
  }

  string wasm_content = util::read_file( argv[2] );
  auto [c_header, h_header] = wasmcompiler::wasm_to_c( argv[1], wasm_content );
  string init;
  if ( argc > 7 ) {
    init = initcomposer::compose_init( argv[1], argv[6], argv[7] );
  } else {
    init = initcomposer::compose_init( argv[1], argv[6], "" );
  }

  ofstream fout_c( argv[3] );
  fout_c << c_header;
  fout_c.close();

  ofstream fout_h( argv[4] );
  fout_h << h_header;
  fout_h.close();

  ofstream fout_init( argv[5] );
  fout_init << init;
  fout_init.close();

  return 0;
}
