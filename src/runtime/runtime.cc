#include "runtime.hh"

using namespace std;

string_view Runtime::getBlob( const string & name )
{
  return name_to_blob_.at( name ).content();
}

string & Runtime::getEncodedBlob( const string & name )
{
  return name_to_encoded_blob_.at( name ).content();
}
