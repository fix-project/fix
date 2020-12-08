#pragma once

#include <string>
#include <string_view>

class Blob {
  private:
    // Name of the blob
    const std::string name_;
    // Content of the blob
    const std::string content_;

  public: 
    // Construct from name and content
    Blob( std::string name, std::string && content ) 
      : name_( name ),
        content_( std::move( content ) )
    {}

    // Construct from content of encoded_blob
    Blob( std::string && encoded_blob_content )
      : content_( std::move( encoded_blob_content ) )
    {
      name_ = sha256::encode( content_ );
    }
                                                         
    std::string_view content() const { return content_; } 
} 


