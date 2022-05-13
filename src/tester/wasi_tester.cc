#include <fstream>
#include <iostream>
#include <string>

#include "name.hh"
#include "runtimestorage.hh"
#include "util.hh"

#include "timing_helper.hh"

using namespace std;

int main( int argc, char* argv[] )
{
  if ( argc < 2 ) {
    cerr << "Usage: " << argv[0] << " path_to_wasi_wasm_file\n";
  }

  string wasm_content = util::read_file( argv[1] );

  auto& runtime = RuntimeStorage::get_instance();

  string program_name( basename( argv[1] ) );
  program_name = program_name.substr( 0, program_name.length() - 5 );
  runtime.add_wasm( program_name, wasm_content );

  vector<Name> encode;
  encode.push_back( Name( "empty" ) );
  encode.push_back( Name( program_name ) );

  Name encode_name = runtime.add_tree( move( encode ) );

  Thunk thunk( encode_name );
  Name thunk_name = runtime.add_thunk( thunk );
  Name res_name = runtime.force_thunk( thunk_name );

  cout << dec;
  cout << "$? is " << *( (const int*)runtime.get_blob( res_name ).data() ) << endl;
  return 0;
}
