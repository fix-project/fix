#pragma once

#include <string>
#include <map>

#include "encode.hh"
#include "wasm-rt.h"

class Invocation {
  private:
    // Name of the program
    std::string program_name_;

    // Map from input name to blob name
    std::map<std::string, std::string> input_to_blob_;

    // Map from output name to blob name
    std::map<std::string, std::string> output_to_blob_;

    // Corresponding memory instance
    wasm_rt_memory_t *mem_;

  public:
    Invocation( const Encode & encode, wasm_rt_memory_t *mem ) 
      : program_name_( encode.program_name_ ),
        input_to_blob_( encode.input_to_blob_ ),
        output_to_blob_( encode.output_to_blob_ ),
        mem_( mem )
    {}
    
    Invocation( const Invocation & ) = default;
    Invocation& operator=( const Invocation & ) = default; 

    // Return whether the varaible is an input
    bool isInput( const std::string & varaible_name );

    // Return blob name corresponds to an input
    std::string getInputBlobName( const std::string & variable_name );

    // Return encoded blob name corresponds to an output
    std::string getOutputBlobName( const std::string & variable_name );

    // Return pointer to wasm memory
    wasm_rt_memory_t * getMem() { return mem_; }
};
    
