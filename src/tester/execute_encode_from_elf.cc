#include <iostream>

#include "runtimestorage.hh"

using namespace std;

int main()
{
  auto & runtime = RuntimeStorage::getInstance();
  
  auto input_blob_one = runtime.addBlob( 1 );
  auto input_blob_two = runtime.addBlob( 2 );
  int blob_content_one;
  int blob_content_two;
  memcpy(&blob_content_one, runtime.getBlob( input_blob_one ).data(), sizeof( int ) );
  memcpy(&blob_content_two, runtime.getBlob( input_blob_two ).data(), sizeof( int ) );
  cout << "Blob content is " << blob_content_one << " and " << blob_content_two << endl;

  return 0;
}
