#pragma once

#include <string>
#include <string_view>

#include "sha256.hh"

class Blob {
  private:
    // Content of the blob
    const std::string content_;

  public: 
    // Construct from name and content
    Blob( std::string && content ) 
      : content_( std::move( content ) )
    {}
                                                         
    std::string_view content() const { return content_; } 
}; 


