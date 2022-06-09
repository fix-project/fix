#include <iostream>
#include <string>

#include "mmap.hh"
#include "name.hh"
#include "runtimestorage.hh"

using namespace std;

int main( int argc, char* argv[] )
{
  if ( argc < 2 ) {
    cerr << "Usage: " << argv[0] << " path_to_wasi_wasm_file\n";
  }

  ReadOnlyFile wasm_content { argv[1] };

  auto& runtime = RuntimeStorage::get_instance();

  Name wasm_name = runtime.add_blob( string_view( wasm_content ) );

  vector<Name> encode;
  encode.push_back( Name( "empty" ) );
  encode.push_back( wasm_name );

  for ( int i = 2; i < argc; i++ ) {
    Name arg_name = runtime.add_blob( string_view( argv[i], strlen( argv[i] ) + 1 ) );
    encode.push_back( arg_name );
  }

  Name encode_name = runtime.add_tree( span_view<Name>( encode.data(), encode.size() ) );

  Thunk thunk( encode_name );
  Name thunk_name = runtime.add_thunk( thunk );
  Name res_name = runtime.force_thunk( thunk_name );

  if ( res_name.is_blob() ) {
    cout << dec;
    cout << "The result is " << *( (const int*)runtime.get_blob( res_name ).data() ) << endl;
    return 0;
  } else {
    auto output_tree = runtime.get_tree( res_name );
    cout << dec;
    cout << "The result is " << *( (const int*)runtime.get_blob( output_tree[0] ).data() ) << endl;
    auto blob_content = runtime.user_get_blob( output_tree[1] );
    if ( blob_content.size() > 0 ) {
      cout << blob_content.data() << std::endl;
    }
    return 0;
  }
}
