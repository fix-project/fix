#include <iostream>
#include <fstream>
#include <string>

#include "ccompiler.hh"
#include "runtimestorage.hh"
#include "wasmcompiler.hh"
#include "util.hh"

using namespace std;

int main( int argc, char * argv[] ) 
{ 
  if ( argc != 4 ) 
  {
    cerr << "Usage: " << argv[0] << " wasm_name path_to_wasm_file path_to_wasm_rt\n";
  }
  
  string wasm_content = util::read_file( argv[2] );
  auto [ c_header, h_header ] = wasmcompiler::wasm_to_c( argv[1], wasm_content );

  string wasm_rt_content = util::read_file( argv[3] );
  string elf_content = c_to_elf( argv[1], c_header, h_header, wasm_rt_content );
  
  auto & runtime = RuntimeStorage::getInstance();
  string program_name = argv[1];

  vector<string> outputsymbols;
  outputsymbols.push_back( "output" );
  runtime.addProgram( program_name, vector<string>(), move( outputsymbols ), elf_content );
    
  // Add encode
  auto encode_name = runtime.addEncode( program_name, vector<string>() );
  runtime.executeEncode( encode_name );

  int output_content;
  memcpy(&output_content, runtime.getBlob( encode_name + "#" + "output" ).data(), sizeof( int ) );
  cout << "Output content is " << output_content << endl;
  
  return 0;
}
     
