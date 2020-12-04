#pragma once

#include <map>
#include <string>
#include <variant>

#include "blob.hh"
#include "encoded_blob.hh"

using TempBlob = std::variant<Blob, EncodedBlob>; 

class BlobFactory {
  private:
    // Map from blob name to blob
    std::map<string, Blob> name_to_blob_;
    // Map from name to encoded blob
    std::map<string, EncodedBlob> name_to_encoded_blob_;
    // Map from encoded blob name to blob name
    std::map<string, string> encoded_blob_to_blob_;

  public:
    // Load blob into BlobFactory if it does not exist yet
    void fetch_blob( const std::string& name );
    
    // Return content of a blob
    const char* read_blob_content( const std::string& name ) const;
    
    // Return pointer to write to a encoded blob
    char* write_encoded_blob_content( const std::string& name );
    
    // Register output of an Encode. Return the name of corresponding encoded blob
    std::string register_encode_output ( const std::string& encode_name, const std::string& output_name );

    // Update output of an Encode
    void update_encode_output ( const std::string& encoded_blob_name );
}


