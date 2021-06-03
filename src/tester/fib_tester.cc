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
    cerr << "Usage: " << argv[0] << " path_to_fib_wasm_file path_to_add_wasm_file arg\n";
  }

  string fib_wasm_content = util::read_file( argv[1] );
  string add_wasm_content = util::read_file( argv[2] );

  auto& runtime = RuntimeStorage::getInstance();

  runtime.addWasm( "fib", fib_wasm_content, { "fib", "add" } );
  runtime.addWasm( "add", add_wasm_content, vector<string>() );

  int arg = atoi( argv[3] );
  Name arg_name = runtime.addBlob( move( string( reinterpret_cast<char*>( &arg ), sizeof( int ) ) ) );

  vector<Name> strict_input;
  strict_input.push_back( arg_name );
  Name strict_input_name = runtime.addTree( move( strict_input ) );

  vector<Name> lazy_input;
  lazy_input.push_back( Name( "fib", NameType::Canonical, ContentType::Blob ) );
  lazy_input.push_back( Name( "add", NameType::Canonical, ContentType::Blob ) );
  Name lazy_input_name = runtime.addTree( move( lazy_input ) );

  Name encode_name = runtime.addEncode(
    Name( "fib", NameType::Canonical, ContentType::Blob ), strict_input_name, lazy_input_name );
  vector<size_t> path = { 0 };

  Thunk res( encode_name, path );
  Name res_name = runtime.forceThunk( res );

  cout << "The result is " << dec << *( (const int*)runtime.getBlob( res_name ).data() ) << endl;

  return 0;
}
