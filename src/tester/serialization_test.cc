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

  Name file_name_blob = runtime.add_blob( string_view( file_name ) );
  Name file_permission_blob = runtime.add_blob( string_view( file_permission ) );
  Name file_content_blob = runtime.add_blob( string_view( file_content ) );
  vector<Name> file_dirent { file_name_blob, file_permission_blob, file_content_blob };
  Name file_dirent_name = runtime.add_tree( span_view<Name>( file_dirent.data(), file_dirent.size() ) );

  vector<Name> home_content { file_dirent_name };
  Name home_content_tree = runtime.add_tree( span_view<Name>( home_content.data(), home_content.size() ) );

  Name home_directory_blob = runtime.add_blob( string_view( home_directory_name ) );
  Name home_permission_blob = runtime.add_blob( string_view( dir_permission ) );
  vector<Name> home_dirent { home_directory_blob, home_permission_blob, home_content_tree };
  Name home_dirent_name = runtime.add_tree( span_view<Name>( home_dirent.data(), home_dirent.size() ) );

  cout << "Serialized home directory: " << runtime.serialize( home_dirent_name ) << endl;

  return 0;
}
