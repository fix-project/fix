#include <fstream>
#include <iostream>
#include <string>

#include "name.hh"
#include "runtimestorage.hh"
#include "util.hh"

using namespace std;

int main( int argc, char* argv[] )
{
  if ( argc < 4 ) {
    cerr << "Usage: " << argv[0] << " path_to_add_wasm_file arg1 arg2\n";
  }

  string wasm_content = util::read_file( argv[1] );

  auto& runtime = RuntimeStorage::getInstance();

  runtime.addWasm( "add", wasm_content, vector<string>() );

  int arg1 = atoi( argv[2] );
  int arg2 = atoi( argv[3] );
  Name arg1_name = runtime.addBlob( move( string( reinterpret_cast<char*>( &arg1 ), sizeof( int ) ) ) );
  Name arg2_name = runtime.addBlob( move( string( reinterpret_cast<char*>( &arg2 ), sizeof( int ) ) ) );

  vector<Name> inputs;
  inputs.push_back( arg1_name );
  inputs.push_back( arg2_name );
  Name strict_input = runtime.addTree( move( inputs ) );

  Name encode_name
    = runtime.addEncode( Name( "add", NameType::Canonical, ContentType::Blob ), strict_input );
  vector<size_t> path = { 0 };

  Thunk res( encode_name, path );
  Name res_name = runtime.forceThunk( res );

  cout << dec;
  cout << "The result is " << *( (const int*)runtime.getBlob( res_name ).data() ) << endl;

  return 0;
}
