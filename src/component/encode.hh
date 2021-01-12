#pragma once

#include <map>
#include <string>

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
};
