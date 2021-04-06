#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "absl/container/flat_hash_map.h"

#include "name.hh"
#include "wasm-rt.h"

struct WasmFileDescriptor 
{
  Name blob_name_;
  uint64_t loc_;
  std::string buffer;

  WasmFileDescriptor( Name blob_name )
    : blob_name_( blob_name ),
      loc_( 0 ),
      buffer( "" )
  {}

  WasmFileDescriptor()
    : blob_name_(),
      loc_( 0 ),
      buffer( "" )
  {} 

};

class Invocation {
  private:
    // Name of the program
    std::string program_name_;

    // Name of encode
    Name encode_name_;

    // Corresponding memory instance
    wasm_rt_memory_t *mem_;

    // Map from fd id to actual fd
    absl::flat_hash_map<int, WasmFileDescriptor> id_to_fd_;

    // vector of the number of strict inputs/lazy inputs
    std::vector<size_t> num_inputs_;

    // The number of all inputs
    size_t input_count_;

  public:
    Invocation( std::string program_name, Name encode_name, wasm_rt_memory_t *mem ) 
      : program_name_( program_name ),
        encode_name_( encode_name ),
        mem_( mem ),
        id_to_fd_(),
        num_inputs_(),
        input_count_( 0 )
    {}

    Invocation( Name encode_name )
      : program_name_( "" ),
        encode_name_( encode_name ),
        mem_( 0 ),
        id_to_fd_(),
        num_inputs_(),
        input_count_( 0 )
    {}
    
    Invocation( const Invocation & ) = default;
    Invocation& operator=( const Invocation & ) = default; 

    // Wet pointer to wasm memory
    void setMem( wasm_rt_memory_t * mem ) { mem_ = mem; }

    // Return pointer to wasm memory
    wasm_rt_memory_t * getMem() { return mem_; }

    // Return file descriptor corresponding to fd_id
    WasmFileDescriptor & getFd( int fd_id );

    // Add file descriptor, return corresponding fd_id
    int addFd( int fd_id, WasmFileDescriptor fd );

    // Open blob corresponding to variable name
    int openVariable( const size_t & index );
    
    static uint64_t next_invocation_id_;

    // Set program name
    void setProgramName( const std::string & program_name ) { program_name_ = program_name; }

    // Return program name
    const std::string & getProgramName() const { return program_name_; }
    
    // Return Name given encode and index of the Name in input Tree
    Name getInputName( const size_t & index );

    size_t getInputCount() const { return input_count_; }
};
    
