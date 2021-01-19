#include "runtimestorage.hh"

using namespace std;

string_view RuntimeStorage::getBlob( const string & name )
{
  return name_to_blob_.get( name ).content();
}

string & RuntimeStorage::getEncodedBlob( const string & name )
{
  return name_to_encoded_blob_.getMutable( name ).content();
}
