#pragma once

#include <map>
#include <string>

#include "blob.hh"
#include "encoded_blob.hh"
#include "storage.hh"

class BlobDecoder {
  private:
    storage<Blob> storage_;
    std::map<std::string, std::string> encoded_blob_to_blob_;
    std::map<std::string, EncodedBlob> name_to_encoded_blob_; 
  public:
    const Blob & get( const std::string & name );
}

