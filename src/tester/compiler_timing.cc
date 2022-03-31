#include <chrono>
#include <fstream>
#include <iostream>
#include <string>

#include "name.hh"
#include "runtimestorage.hh"
#include "timing_helper.hh"
#include "util.hh"

using namespace std;

int main( int argc, char* argv[] )
{
  if ( argc != 3 ) {
    cerr << "Usage: " << argv[0] << " path_to_add_wasm_file num_of_iterations\n";
  }

  string wasm_content = util::read_file( argv[1] );

  auto& runtime = RuntimeStorage::getInstance();

  runtime.addWasm( "add", wasm_content, vector<string>() );

  int iterations = atoi( argv[2] );
  for ( int i = 0; i < iterations; i++ ) {
    int arg1 = i;
    int arg2 = i + 1;
    Name arg1_name = runtime.addBlob( move( string( reinterpret_cast<char*>( &arg1 ), sizeof( int ) ) ) );
    Name arg2_name = runtime.addBlob( move( string( reinterpret_cast<char*>( &arg2 ), sizeof( int ) ) ) );

    vector<Name> inputs;
    inputs.push_back( arg1_name );
    inputs.push_back( arg2_name );
    Name strict_input = runtime.addTree( move( inputs ) );

    Name encode_name = runtime.addEncode( Name( "add", NameType::Canonical, ContentType::Blob ), strict_input );
    vector<size_t> path = { 0 };

    Thunk res( encode_name, path );
    runtime.forceThunk( res );
  }

  cout << dec;
  print_timer( cout, "_invocation_init", _invocation_init );
  print_timer( cout, "_memory_init_cheap", _memory_init_cheap );
  return 0;
}
