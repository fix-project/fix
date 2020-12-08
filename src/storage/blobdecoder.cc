#include "blobdecoder.hh"
#include "sha256.hh"

using namespace std;

const Blob& BlobDecoder::get( const std::string& name ) 
{
  auto it = encoded_blob_to_blob_.find( name );  
  if ( it != encoded_blob_to_blob_.end() )
  {
    return storage_.get( encoded_blob_to_blob_[ name ] );
  }
 
  // TODO: execute corresponding ENCODE
 
  string name = sha256::encode( name_to_encoded_blob_[ name ].content() );
  storage_.put( name, name_to_encoded_blob_[ name ].content() );
  return storage_.get( name );
}
