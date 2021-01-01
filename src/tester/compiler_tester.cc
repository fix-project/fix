#include <iostream>
#include <fstream>
#include <string>

#include "wasmcompiler.hh"
#include "util.hh"

using namespace std;

int main( int argc, char * argv[] ) 
{ 
  if ( argc <= 1 )
  {
    cerr << "Usage: " << argv[0] << " path_to_wasm_file <output_path_of_c_file>\n";
  }
  
  string wasm_content = util::read_file( argv[1] );
  auto [ c_header, h_header ] = wasmcompiler::wasm_to_c( argv[1], wasm_content );
  cout << c_header << endl;
  
  if ( argc == 3 ) 
  { 
    ofstream fout ( argv[2] );
    fout << c_header << endl;
    fout.close();
  } 
  return 0;
}
     
