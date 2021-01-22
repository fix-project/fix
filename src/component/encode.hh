#pragma once

#include <map>
#include <string>
#include <vector>

/**
 * Structure of Encode:
 * --------------------------------
 *  header:name of program
 *         number of input entries
 *         number of output entries
 * --------------------------------
 *        Input entries
 * --------------------------------
 *        Output entries
 * --------------------------------
 *        Wasm module
 * --------------------------------
 */
class Encode {
  private:
    // Name of the encode 
    std::string name_;
    // Name of the program
    std::string program_name_;
    // List of named input symbols
    std::map<std::string, std::string> input_to_blob_;
    // List of named output symbols
    std::map<std::string, std::string> output_to_blob_;

    friend class Invocation;
    
  public:
    Encode( const std::string & program_name, std::map<std::string, std::string> && input_to_blob, const std::vector<std::string> & output_symbols );

    std::string name() { return name_; }

    const std::map<std::string, std::string> & getOutputBlobNames() { return output_to_blob_; }
};
