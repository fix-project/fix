#include <iostream>
#include <string>

#include "mmap.hh"
#include "name.hh"
#include "runtimestorage.hh"

using namespace std;

int main( int argc, char* argv[] )
{
  if ( argc < 4 ) {
    cerr << "Usage: " << argv[0] << " path_to_add_wasm_file arg1 arg2\n";
  }

  ReadOnlyFile wasm_content { argv[1] };

  auto& runtime = RuntimeStorage::get_instance();

  Name wasm_name = runtime.add_blob( string_view( wasm_content ) );
  Name arg1_name = runtime.add_blob( make_blob( atoi( argv[2] ) ) );
  Name arg2_name = runtime.add_blob( make_blob( atoi( argv[3] ) ) );

  vector<Name> encode;
  encode.push_back( Name( "empty" ) );
  encode.push_back( wasm_name );
  encode.push_back( arg1_name );
  encode.push_back( arg2_name );

  Name encode_name = runtime.add_tree( Tree::make_tree( encode ) );

  Thunk thunk( encode_name );
  Name thunk_name = runtime.add_thunk( thunk );
  Name res_name = runtime.force_thunk( thunk_name );

  cout << dec;
  cout << "The result is " << *( (const int*)runtime.get_blob( res_name ).data() ) << endl;
  return 0;
}
