#include <chrono>
#include <iostream>
#include <fstream>
#include <string>

#include "ccompiler.hh"
#include "runtimestorage.hh"
#include "timing_helper.hh"
#include "util.hh"
#include "wasmcompiler.hh"

using namespace std;

int main( int argc, char * argv[] ) 
{ 
  if ( argc < 5 ) 
  {
    cerr << "Usage: " << argv[0] << " wasm_name path_to_wasm_file path_to_wasm_rt #_executions [arg1] [arg2]\n";
  }
  
  int arg1 = 0;
  int arg2 = 0;

  if ( argc == 6 ) {
    arg1 = atoi( argv[5] );
  } else if ( argc == 7 ) {
    arg1 = atoi( argv[5] );
    arg2 = atoi( argv[6] );
  }

  int num_execution = atoi( argv[4] );

  auto compile_start = chrono::high_resolution_clock::now();
  
  string wasm_content = util::read_file( argv[2] );
  auto [ c_header, h_header ] = wasmcompiler::wasm_to_c( argv[1], wasm_content );

  string wasm_rt_content = util::read_file( argv[3] );
  string elf_content = c_to_elf( argv[1], c_header, h_header, wasm_rt_content );
  
  auto & runtime = RuntimeStorage::getInstance();
  string program_name = argv[1];

  vector<string> outputsymbols;
  outputsymbols.push_back( "output" );
  runtime.addProgram( program_name, vector<string>(), move( outputsymbols ), elf_content );
  auto encode_name = runtime.addEncode( program_name, vector<string>() );
  
  auto start = chrono::high_resolution_clock::now();
  
  for ( int i = 0; i < num_execution; i++ )
  {
    runtime.executeEncode( encode_name, arg1, arg2 );
  }
 
  auto stop = chrono::high_resolution_clock::now();
  
  auto compile_duration = chrono::duration_cast<chrono::microseconds>( start - compile_start );
  auto execution_duration = chrono::duration_cast<chrono::microseconds>( stop - start );
  cout << "Executing " << num_execution << " times takes " << compile_duration.count() << " microseconds to compile "
	 <<  execution_duration.count() << " microseconds to execute " << endl; 

  print_timer( cout, "_mmap ", _mmap );
  return 0;
}
     
