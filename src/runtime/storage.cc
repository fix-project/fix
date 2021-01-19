#include "storage.hh"

using namespace std;

string_view Storage::getBlob( const string & name )
{
  return name_to_blob_.at( name ).content();
}

string & Storage::getEncodedBlob( const string & name )
{
  return name_to_encoded_blob_.at( name ).content();
}
