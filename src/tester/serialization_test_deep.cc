#include <iostream>
#include <string>

#include "runtimestorage.hh"

using namespace std;

int main()
{
  auto& runtime = RuntimeStorage::get_instance();

  string dir_permission = "040000";
  string file_permission = "100644";
  string home_directory_name = "."; // . contains 1 directory: a

  // a/b/c/fixpoint
  string directory_a_name = "a"; // a contains 2 files: a_1.txt and a_2.txt AND 1 directory: b
  string a_file_1_name = "a_1.txt";
  string a_file_2_name = "a_2.txt";
  string a_file_1_content = "Hello, this is a_1.txt!";
  string a_file_2_content = "Hello, this is a_2.txt!";

  string directory_b_name = "b"; // b contains 1 file: b_1.txt AND 2 directories: hello and c
  string b_file_1_name = "b_1.txt";
  string b_file_1_content = "this is b_1.txt!";
  string b_directory_1_name = "hello"; // hello contains 1 file: greeter.txt
  string hello_file_1_name = "greeter.txt";
  string hello_file_1_content = "Hi, I am greeter.txt";

  string directory_c_name = "c"; // c contains 1 file: fixpoint
  string c_file_1_name = "fixpoint";
  string c_file_1_content = "Hello, world!";

  Name file_permission_blob
    = runtime.add_blob( string_view( file_permission ) ); // all file dirents will have the same permissions
  Name dir_permission_blob
    = runtime.add_blob( string_view( dir_permission ) ); // all dir dirents will have the same permissions

  Name c_file_1_name_blob = runtime.add_blob( string_view( c_file_1_name ) );
  Name c_file_1_content_blob = runtime.add_blob( string_view( c_file_1_content ) );
  vector<Name> c_file_1_dirent { c_file_1_name_blob, file_permission_blob, c_file_1_content_blob };
  Name c_file_1_dirent_name = runtime.add_tree( span_view<Name>( c_file_1_dirent.data(), c_file_1_dirent.size() ) );

  vector<Name> c_content { c_file_1_dirent_name };
  Name c_content_tree = runtime.add_tree( span_view<Name>( c_content.data(), c_content.size() ) );

  Name directory_c_name_blob = runtime.add_blob( string_view( directory_c_name ) );
  vector<Name> directory_c_dirent = { directory_c_name_blob, dir_permission_blob, c_content_tree };
  Name directory_c_dirent_name
    = runtime.add_tree( span_view<Name>( directory_c_dirent.data(), directory_c_dirent.size() ) );

  Name hello_file_1_name_blob = runtime.add_blob( string_view( hello_file_1_name ) );
  Name hello_file_1_content_blob = runtime.add_blob( string_view( hello_file_1_content ) );
  vector<Name> hello_file_1_dirent { hello_file_1_name_blob, file_permission_blob, hello_file_1_content_blob };
  Name hello_file_1_dirent_name
    = runtime.add_tree( span_view<Name>( hello_file_1_dirent.data(), hello_file_1_dirent.size() ) );

  vector<Name> hello_content { hello_file_1_dirent_name };
  Name hello_content_tree = runtime.add_tree( span_view<Name>( hello_content.data(), hello_content.size() ) );

  Name b_directory_1_name_blob = runtime.add_blob( string_view( b_directory_1_name ) );
  vector<Name> b_directory_1_dirent = { b_directory_1_name_blob, dir_permission_blob, hello_content_tree };
  Name b_directory_1_dirent_name
    = runtime.add_tree( span_view<Name>( b_directory_1_dirent.data(), b_directory_1_dirent.size() ) );

  Name b_file_1_name_blob = runtime.add_blob( string_view( b_file_1_name ) );
  Name b_file_1_content_blob = runtime.add_blob( string_view( b_file_1_content ) );
  vector<Name> b_file_1_dirent = { b_file_1_name_blob, file_permission_blob, b_file_1_content_blob };
  Name b_file_1_dirent_name = runtime.add_tree( span_view<Name>( b_file_1_dirent.data(), b_file_1_dirent.size() ) );

  vector<Name> b_content { b_file_1_dirent_name, directory_c_dirent_name, b_directory_1_dirent_name };
  Name b_content_tree = runtime.add_tree( span_view<Name>( b_content.data(), b_content.size() ) );

  Name directory_b_name_blob = runtime.add_blob( string_view( directory_b_name ) );
  vector<Name> directory_b_dirent = { directory_b_name_blob, dir_permission_blob, b_content_tree };
  Name directory_b_dirent_name
    = runtime.add_tree( span_view<Name>( directory_b_dirent.data(), directory_b_dirent.size() ) );

  Name a_file_1_name_blob = runtime.add_blob( string_view( a_file_1_name ) );
  Name a_file_1_content_blob = runtime.add_blob( string_view( a_file_1_content ) );
  vector<Name> a_file_1_dirent { a_file_1_name_blob, file_permission_blob, a_file_1_content_blob };
  Name a_file_1_dirent_name = runtime.add_tree( span_view<Name>( a_file_1_dirent.data(), a_file_1_dirent.size() ) );

  Name a_file_2_name_blob = runtime.add_blob( string_view( a_file_2_name ) );
  Name a_file_2_content_blob = runtime.add_blob( string_view( a_file_2_content ) );
  vector<Name> a_file_2_dirent { a_file_2_name_blob, file_permission_blob, a_file_2_content_blob };
  Name a_file_2_dirent_name = runtime.add_tree( span_view<Name>( a_file_2_dirent.data(), a_file_2_dirent.size() ) );

  vector<Name> a_content { directory_b_dirent_name, a_file_1_dirent_name, a_file_2_dirent_name };
  Name a_content_tree = runtime.add_tree( span_view<Name>( a_content.data(), a_content.size() ) );

  Name directory_a_name_blob = runtime.add_blob( string_view( directory_a_name ) );
  vector<Name> directory_a_dirent { directory_a_name_blob, dir_permission_blob, a_content_tree };
  Name directory_a_dirent_name
    = runtime.add_tree( span_view<Name>( directory_a_dirent.data(), directory_a_dirent.size() ) );

  vector<Name> home_content { directory_a_dirent_name };
  Name home_content_tree = runtime.add_tree( span_view<Name>( home_content.data(), home_content.size() ) );

  Name home_directory_blob = runtime.add_blob( string_view( home_directory_name ) );
  vector<Name> home_dirent { home_directory_blob, dir_permission_blob, home_content_tree };
  Name home_dirent_name = runtime.add_tree( span_view<Name>( home_dirent.data(), home_dirent.size() ) );

  cout << "Serialized home directory: " << runtime.serialize( home_dirent_name ) << endl;

  return 0;
}
