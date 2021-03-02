#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "absl/container/flat_hash_map.h"

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

    // Name of encode
    std::string encode_name_;

    // Corresponding memory instance
    wasm_rt_memory_t *mem_;

    // Map from fd id to actual fd
    std::vector<WasmFileDescriptor> id_to_fd_;

  public:
    Invocation( const Encode & encode, wasm_rt_memory_t *mem ) 
      : program_name_( encode.program_name_ ),
        encode_name_( encode.name_ ),
        mem_( mem ),
        id_to_fd_()
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
    WasmFileDescriptor & getFd( int fd_id );

    // Add file descriptor, return corresponding fd_id
    int addFd( WasmFileDescriptor fd );

    // Open blob corresponding to variable name
    int openVariable( const std::string & variable_name );
    
    static uint64_t next_invocation_id_;
};
    
