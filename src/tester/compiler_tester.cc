#include <iostream>
#include <string>

#include "wasmcompiler.hh"
#include "util.hh"

using namespace std;

int main( int argc, char * argv[] ) 
{ 
  cout << "Hello world" << endl;
  if ( argc == 2 ) 
  { 
    string wasm_content = util::read_file( argv[1] );
    auto [ c_header, h_header ] = wasmcompiler::wasm_to_c( argv[1], wasm_content );
    cout <<  c_header << endl;
    cout << h_header << endl;
  } 
  return 0;
}
     
