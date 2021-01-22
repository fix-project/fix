#pragma once

#include <string>
#include <map>

#include "encode.hh"
#include "wasm-rt.h"

enum class fd_mode
{
  BLOB,
  ENCODEDBLOB
};

struct WasmFileDescriptor 
{
  std::string blob_name_;
  uint64_t loc_;
  fd_mode mode_;

  WasmFileDescriptor( std::string blob_name, fd_mode mode )
    : blob_name_( blob_name ),
      loc_( 0 ),
      mode_( mode )
  {}

};

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

    // Map from fd id to actual fd
    std::map<uint64_t, WasmFileDescriptor> id_to_fd_;

    // Next available fd id;
    uint64_t next_fd_id_;
  

  public:
    Invocation( const Encode & encode, wasm_rt_memory_t *mem ) 
      : program_name_( encode.program_name_ ),
        input_to_blob_( encode.input_to_blob_ ),
        output_to_blob_( encode.output_to_blob_ ),
        mem_( mem ),
        id_to_fd_(),
        next_fd_id_( 0 )
    {}
    
    Invocation( const Invocation & ) = default;
    Invocation& operator=( const Invocation & ) = default; 

    // Return whether the varaible is an input
    bool isInput( const std::string & varaible_name );

    // Return whether the varaible is an output
    bool isOutput( const std::string & varaible_name );

    // Return blob name corresponds to an input
    std::string getInputBlobName( const std::string & variable_name );

    // Return encoded blob name corresponds to an output
    std::string getOutputBlobName( const std::string & variable_name );

    // Return pointer to wasm memory
    wasm_rt_memory_t * getMem() { return mem_; }

    // Return file descriptor corresponding to fd_id
    WasmFileDescriptor & getFd( uint64_t fd_id );

    // Add file descriptor, return corresponding fd_id
    uint64_t addFd( WasmFileDescriptor fd );

    // Open blob corresponding to variable name
    uint64_t openVariable( const std::string & variable_name );
    
    static uint64_t next_invocation_id_;
};
    
