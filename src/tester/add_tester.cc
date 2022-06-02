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
  if ( argc < 4 ) {
    cerr << "Usage: " << argv[0] << " path_to_add_wasm_file arg1 arg2\n";
  }

  string wasm_content = util::read_file( argv[1] );

  auto& runtime = RuntimeStorage::get_instance();

  runtime.add_wasm( "addblob", wasm_content );

  int arg1 = atoi( argv[2] );
  int arg2 = atoi( argv[3] );
  Name arg1_name = runtime.add_blob( Blob( move( string( reinterpret_cast<char*>( &arg1 ), sizeof( int ) ) ) ) );
  Name arg2_name = runtime.add_blob( Blob( move( string( reinterpret_cast<char*>( &arg2 ), sizeof( int ) ) ) ) );

  vector<Name> encode;
  encode.push_back( Name( "empty" ) );
  encode.push_back( Name( "addblob" ) );
  encode.push_back( arg1_name );
  encode.push_back( arg2_name );

  Name encode_name = runtime.add_tree( move( encode ) );

  Thunk thunk( encode_name );
  Name thunk_name = runtime.add_thunk( thunk );
  Name res_name = runtime.force_thunk( thunk_name );

  cout << dec;
  cout << "The result is " << *( (const int*)runtime.get_blob( res_name ).data() ) << endl;
  return 0;
}
