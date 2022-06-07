#include <iostream>
#include <string>

#include "mmap.hh"
#include "name.hh"
#include "runtimestorage.hh"

using namespace std;

int main( int argc, char* argv[] )
{
  if ( argc < 4 ) {
    cerr << "Usage: " << argv[0] << " path_to_fib_wasm_file path_to_add_wasm_file arg\n";
  }

  ReadOnlyFile fib_wasm_content { argv[1] };
  ReadOnlyFile add_wasm_content { argv[2] };

  auto& runtime = RuntimeStorage::get_instance();

  runtime.add_wasm( "fib", fib_wasm_content );
  runtime.add_wasm( "add", add_wasm_content );

  Name arg_name = runtime.add_blob( make_blob( atoi( argv[3] ) ) );

  vector<Name> encode;
  encode.push_back( Name( "empty" ) );
  encode.push_back( Name( "fib" ) );
  encode.push_back( arg_name );
  encode.push_back( Name( "add" ) );
  Name encode_name = runtime.add_tree( Tree::make_tree( encode ) );

  Thunk thunk( encode_name );
  Name thunk_name = runtime.add_thunk( thunk );
  Name res_name = runtime.force_thunk( thunk_name );

  cout << dec;
  cout << "The result is " << *( (const int*)runtime.get_blob( res_name ).data() ) << endl;
  return 0;
}
