#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "absl/container/flat_hash_map.h"

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
    absl::flat_hash_map<std::string, std::string> input_to_blob_;
    // List of named output symbols
    absl::flat_hash_map<std::string, std::string> output_to_blob_;

    friend class Invocation;
    
  public:
    Encode( const std::string & program_name, absl::flat_hash_map<std::string, std::string> && input_to_blob, const std::vector<std::string> & output_symbols );

    std::string name() { return name_; }

    const absl::flat_hash_map<std::string, std::string> & getOutputBlobNames() const { return output_to_blob_; }
   
    const absl::flat_hash_map<std::string, std::string> & getInputBlobNames() const { return input_to_blob_; }

    const std::string & getProgramName() const { return program_name_; }
};
