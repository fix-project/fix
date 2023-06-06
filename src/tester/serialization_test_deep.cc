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

  Handle file_permission_blob
    = runtime.add_blob( string_view( file_permission ) ); // all file dirents will have the same permissions
  Handle dir_permission_blob
    = runtime.add_blob( string_view( dir_permission ) ); // all dir dirents will have the same permissions

  Handle c_file_1_name_blob = runtime.add_blob( string_view( c_file_1_name ) );
  Handle c_file_1_content_blob = runtime.add_blob( string_view( c_file_1_content ) );
  vector<Handle> c_file_1_dirent { c_file_1_name_blob, file_permission_blob, c_file_1_content_blob };
  Handle c_file_1_dirent_name
    = runtime.add_tree( span_view<Handle>( c_file_1_dirent.data(), c_file_1_dirent.size() ) );

  vector<Handle> c_content { c_file_1_dirent_name };
  Handle c_content_tree = runtime.add_tree( span_view<Handle>( c_content.data(), c_content.size() ) );

  Handle directory_c_name_blob = runtime.add_blob( string_view( directory_c_name ) );
  vector<Handle> directory_c_dirent = { directory_c_name_blob, dir_permission_blob, c_content_tree };
  Handle directory_c_dirent_name
    = runtime.add_tree( span_view<Handle>( directory_c_dirent.data(), directory_c_dirent.size() ) );

  Handle hello_file_1_name_blob = runtime.add_blob( string_view( hello_file_1_name ) );
  Handle hello_file_1_content_blob = runtime.add_blob( string_view( hello_file_1_content ) );
  vector<Handle> hello_file_1_dirent { hello_file_1_name_blob, file_permission_blob, hello_file_1_content_blob };
  Handle hello_file_1_dirent_name
    = runtime.add_tree( span_view<Handle>( hello_file_1_dirent.data(), hello_file_1_dirent.size() ) );

  vector<Handle> hello_content { hello_file_1_dirent_name };
  Handle hello_content_tree = runtime.add_tree( span_view<Handle>( hello_content.data(), hello_content.size() ) );

  Handle b_directory_1_name_blob = runtime.add_blob( string_view( b_directory_1_name ) );
  vector<Handle> b_directory_1_dirent = { b_directory_1_name_blob, dir_permission_blob, hello_content_tree };
  Handle b_directory_1_dirent_name
    = runtime.add_tree( span_view<Handle>( b_directory_1_dirent.data(), b_directory_1_dirent.size() ) );

  Handle b_file_1_name_blob = runtime.add_blob( string_view( b_file_1_name ) );
  Handle b_file_1_content_blob = runtime.add_blob( string_view( b_file_1_content ) );
  vector<Handle> b_file_1_dirent = { b_file_1_name_blob, file_permission_blob, b_file_1_content_blob };
  Handle b_file_1_dirent_name
    = runtime.add_tree( span_view<Handle>( b_file_1_dirent.data(), b_file_1_dirent.size() ) );

  vector<Handle> b_content { b_file_1_dirent_name, directory_c_dirent_name, b_directory_1_dirent_name };
  Handle b_content_tree = runtime.add_tree( span_view<Handle>( b_content.data(), b_content.size() ) );

  Handle directory_b_name_blob = runtime.add_blob( string_view( directory_b_name ) );
  vector<Handle> directory_b_dirent = { directory_b_name_blob, dir_permission_blob, b_content_tree };
  Handle directory_b_dirent_name
    = runtime.add_tree( span_view<Handle>( directory_b_dirent.data(), directory_b_dirent.size() ) );

  Handle a_file_1_name_blob = runtime.add_blob( string_view( a_file_1_name ) );
  Handle a_file_1_content_blob = runtime.add_blob( string_view( a_file_1_content ) );
  vector<Handle> a_file_1_dirent { a_file_1_name_blob, file_permission_blob, a_file_1_content_blob };
  Handle a_file_1_dirent_name
    = runtime.add_tree( span_view<Handle>( a_file_1_dirent.data(), a_file_1_dirent.size() ) );

  Handle a_file_2_name_blob = runtime.add_blob( string_view( a_file_2_name ) );
  Handle a_file_2_content_blob = runtime.add_blob( string_view( a_file_2_content ) );
  vector<Handle> a_file_2_dirent { a_file_2_name_blob, file_permission_blob, a_file_2_content_blob };
  Handle a_file_2_dirent_name
    = runtime.add_tree( span_view<Handle>( a_file_2_dirent.data(), a_file_2_dirent.size() ) );

  vector<Handle> a_content { directory_b_dirent_name, a_file_1_dirent_name, a_file_2_dirent_name };
  Handle a_content_tree = runtime.add_tree( span_view<Handle>( a_content.data(), a_content.size() ) );

  Handle directory_a_name_blob = runtime.add_blob( string_view( directory_a_name ) );
  vector<Handle> directory_a_dirent { directory_a_name_blob, dir_permission_blob, a_content_tree };
  Handle directory_a_dirent_name
    = runtime.add_tree( span_view<Handle>( directory_a_dirent.data(), directory_a_dirent.size() ) );

  vector<Handle> home_content { directory_a_dirent_name };
  Handle home_content_tree = runtime.add_tree( span_view<Handle>( home_content.data(), home_content.size() ) );

  Handle home_directory_blob = runtime.add_blob( string_view( home_directory_name ) );
  vector<Handle> home_dirent { home_directory_blob, dir_permission_blob, home_content_tree };
  Handle home_dirent_name = runtime.add_tree( span_view<Handle>( home_dirent.data(), home_dirent.size() ) );

  cout << "Serialized home directory: " << runtime.serialize( home_dirent_name ) << endl;

  return 0;
}
