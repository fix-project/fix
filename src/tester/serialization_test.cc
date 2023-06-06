#include <iostream>
#include <string>

#include "runtimestorage.hh"

using namespace std;

int main()
{
  auto& runtime = RuntimeStorage::get_instance();

  string dir_permission = "040000";
  string file_permission = "100644";
  string home_directory_name = ".";
  string file_name = "fixpoint";
  string file_content = "Hello, world!";

  Handle file_name_blob = runtime.add_blob( string_view( file_name ) );
  Handle file_permission_blob = runtime.add_blob( string_view( file_permission ) );
  Handle file_content_blob = runtime.add_blob( string_view( file_content ) );
  vector<Handle> file_dirent { file_name_blob, file_permission_blob, file_content_blob };
  Handle file_dirent_name = runtime.add_tree( span_view<Handle>( file_dirent.data(), file_dirent.size() ) );

  vector<Handle> home_content { file_dirent_name };
  Handle home_content_tree = runtime.add_tree( span_view<Handle>( home_content.data(), home_content.size() ) );

  Handle home_directory_blob = runtime.add_blob( string_view( home_directory_name ) );
  Handle home_permission_blob = runtime.add_blob( string_view( dir_permission ) );
  vector<Handle> home_dirent { home_directory_blob, home_permission_blob, home_content_tree };
  Handle home_dirent_name = runtime.add_tree( span_view<Handle>( home_dirent.data(), home_dirent.size() ) );

  cout << "Serialized home directory: " << runtime.serialize( home_dirent_name ) << endl;

  return 0;
}
