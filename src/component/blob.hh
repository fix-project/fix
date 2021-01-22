#pragma once

#include <string>
#include <string_view>

#include "sha256.hh"

class Blob {
  private:
    // Content of the blob
    const std::string content_;
    // Name of the blob
    const std::string name_;

  public: 
    // Construct from name and content
    Blob( std::string name, std::string && content ) 
      : content_( std::move( content ) ),
        name_( name )
    {}

    // Construct from content of encoded_blob
    Blob( std::string && encoded_blob_content )
      : content_( std::move( encoded_blob_content ) ),
        name_( sha256::encode( content_ ) )
    {}
                                                         
    std::string_view content() const { return content_; } 

    std::string name() const { return name_; }
}; 


