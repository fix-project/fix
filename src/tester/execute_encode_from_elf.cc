#include <iostream>

#include "runtimestorage.hh"
#include "util.hh"

using namespace std;

int main( int argc, char* argv[] )
{
  auto & runtime = RuntimeStorage::getInstance();
  
  // Add input blobs
  auto input_blob_one = runtime.addBlob( 1 );
  auto input_blob_two = runtime.addBlob( 2 );
  int blob_content_one;
  int blob_content_two;
  memcpy(&blob_content_one, runtime.getBlob( input_blob_one ).data(), sizeof( int ) );
  memcpy(&blob_content_two, runtime.getBlob( input_blob_two ).data(), sizeof( int ) );
  cout << "Blob content is " << blob_content_one << " and " << blob_content_two << endl;

  if ( argc >= 1 )
  { 
    // Add program
    string program_name = string( argv[1] );
    auto elf_content = util::read_file( argv[1] );
    runtime.addProgram( program_name, vector<string>(), vector<string>(), elf_content );
    
    // Add encode
    auto encode_name = runtime.addEncode( program_name, vector<string>() );
    runtime.executeEncode( encode_name );
  }
    
  return 0;
}
