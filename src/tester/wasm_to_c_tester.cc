#include <fstream>
#include <iostream>
#include <string>

#include "ccompiler.hh"
#include "initcomposer.hh"
#include "mmap.hh"
#include "wasmcompiler.hh"

using namespace std;

int main( int argc, char* argv[] )
{
  if ( argc != 6 ) {
    cerr << "Usage: " << argv[0] << " wasm_name path_to_wasm_file path_to_c path_to_h path_to_fixpoint_header\n";
  }

  ReadOnlyFile wasm_content { argv[2] };

  auto [c_header, h_header, fixpoint_header] = wasmcompiler::wasm_to_c( wasm_content );

  ofstream fout_c( argv[3] );
  fout_c << c_header;
  fout_c.close();

  ofstream fout_h( argv[4] );
  fout_h << h_header;
  fout_h.close();

  ofstream fout_fixpoint( argv[5] );
  fout_fixpoint << fixpoint_header;
  fout_fixpoint.close();

  return 0;
}
