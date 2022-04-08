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

  auto& runtime = RuntimeStorage::getInstance();

  runtime.addWasm( "addblob", wasm_content );

  int N = atoi(argv[3]);
  for (int i = 0; i < N; i++) {
    int arg1 = atoi( argv[2] );
    int arg2 = i;
    Name arg1_name = runtime.addBlob( move( string( reinterpret_cast<char*>( &arg1 ), sizeof( int ) ) ) );
    Name arg2_name = runtime.addBlob( move( string( reinterpret_cast<char*>( &arg2 ), sizeof( int ) ) ) );
  
    vector<TreeEntry> encode;
    encode.push_back( TreeEntry( Name("empty", NameType::Canonical, ContentType::Blob ), Laziness::Strict ));
    encode.push_back( TreeEntry( Name("addblob", NameType::Canonical, ContentType::Blob ), Laziness::Strict) );
    encode.push_back( TreeEntry(arg1_name, Laziness::Strict) );
    encode.push_back( TreeEntry(arg2_name, Laziness::Strict) );
  
    Name encode_name = runtime.addTree( move(encode) );
  
    Thunk thunk ( encode_name );
    Name thunk_name = runtime.addThunk( thunk );
    Name res_name = runtime.forceThunk( thunk_name );
  
    //cout << dec;
    //cout << "The result is " << *( (const int*)runtime.getBlob( res_name ).data() ) << endl;
  } 
  
  print_timer( cout, "_fixpoint_apply", _fixpoint_apply );
  //print_timer( cout, "_attach_blob", _attach_blob);
  //print_timer(cout, "_get_tree_entry", _get_tree_entry);
  //print_timer(cout, "_detach_mem", _detach_mem);
  //print_timer(cout, "_freeze_blob", _freeze_blob);
  //print_timer(cout, "_designate_output", _designate_output);
  return 0;
}
