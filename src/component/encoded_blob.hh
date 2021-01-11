#pragma once

#include <string>

/**
 * ----
 *  Magic String
 * ----
 *  Name of ENCODE
 * ----
 *  Symbolic name of output
 * ----
 *  Output of the ENCODE
 * ----
 */

class EncodedBlob {
  private:
    // Name of the blob
    std::string name_;
    // Name of the Encode
    std::string encode_name_;
    // Symbolic name of output
    std::string output_name_;
    // Content of the blob
    std::string content_;
    
  public: 
    // Constructor
    EncodedBlob( std::string encode_name, std::string output_name ) 
      : name_( encode_name + "#" + output_name ),
        encode_name_( encode_name ),
        output_name_( output_name ),
        content_( "" )
    {}

    std::string_view content() { return content_; } 
}; 


