#include <iostream>
#include <string>

#include "base64.hh"
#include "runtimestorage.hh"

using namespace std;

int main( int argc, char* argv[] )
{
  if ( argc < 2 ) {
    cerr << "Usage: " << argv[0] << " base64_name" << endl;
  }
  string base64_name( argv[1] );

  auto& runtime = RuntimeStorage::get_instance();
  runtime.deserialize();

  Name name( base64::decode( base64_name ) );
  auto tree = runtime.get_tree( name );
  auto dirent_tree = runtime.get_tree( tree[2] );
  auto file_dirent = runtime.get_tree( dirent_tree[0] );
  auto file_content = runtime.user_get_blob( file_dirent[2] );

  cout << file_content.data() << endl;

  return 0;
}
