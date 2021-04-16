#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "absl/container/flat_hash_map.h"

#include "name.hh"
#include "wasm-rt.h"

#define BLOB 0
#define TREE 1
#define THUNK 2

struct InputMem
{
  Name name_;

  InputMem( Name name )
    : name_( name )
  {}
};

struct OutputTemp
{
  Name name_;
  
  std::vector<OutputTemp> childs;
  std::string buffer;
  OutputTemp* encode;
  std::vector<int> path;
 
  OutputTemp( Name name )
    : name_( name ),
      childs(),
      buffer( "" ),
      encode(),
      path()
  {}

  OutputTemp( ContentType content_type )
    : name_( content_type ),
      childs(),
      buffer( "" ),
      encode(),
      path()
  {}

  OutputTemp( uint32_t content_type )
    : name_(),
      childs(),
      buffer( "" ),
      encode(),
      path()
  {
    switch ( content_type ) 
    {
      case BLOB : 
        name_ = Name ( ContentType::Blob );

      case TREE :
        name_ = Name ( ContentType::Tree );

      case THUNK :
        name_ = Name ( ContentType::Thunk );
        
      default :
        throw std::runtime_error ( "Invalid argument." );
    }
  }

  OutputTemp( const OutputTemp & temp )
    : name_( temp.name_ ),
      childs( temp.childs ),
      buffer( temp.buffer ),
      encode( temp.encode ),
      path( temp.path )
  {}

  OutputTemp& operator=( const OutputTemp& temp ) = default;
};

class Invocation {
  private:
    // Name of the program
    std::string program_name_;

    // Name of encode
    Name encode_name_;

    // Corresponding memory instance
    wasm_rt_memory_t *mem_;

    // Map from id to input mem
    absl::flat_hash_map<uint32_t, InputMem> input_mems;

    // Temporary output tree
    OutputTemp output_tree_;

    // Map from id to output mems
    absl::flat_hash_map<uint32_t, OutputTemp*> output_mems;

    // vector of the number of strict inputs/lazy inputs
    std::vector<size_t> num_inputs_;

    // The number of all inputs
    size_t input_count_;

  public:
    Invocation( std::string program_name, Name encode_name, wasm_rt_memory_t *mem ) 
      : program_name_( program_name ),
        encode_name_( encode_name ),
        mem_( mem ),
        input_mems(),
        output_tree_( ContentType::Tree ),
        output_mems(),
        num_inputs_(),
        input_count_( 0 )
    {}

    Invocation( Name encode_name )
      : program_name_( "" ),
        encode_name_( encode_name ),
        mem_( 0 ),
        input_mems(),
        output_tree_( ContentType::Tree ),
        output_mems(),
        num_inputs_(),
        input_count_( 0 )
    {}
    
    Invocation( const Invocation & ) = default;
    Invocation& operator=( const Invocation & ) = default; 

    // Wet pointer to wasm memory
    void setMem( wasm_rt_memory_t * mem ) { mem_ = mem; }

    // Return pointer to wasm memory
    wasm_rt_memory_t * getMem() { return mem_; }
    
    static uint64_t next_invocation_id_;

    // Set program name
    void setProgramName( const std::string & program_name ) { program_name_ = program_name; }

    // Return program name
    const std::string & getProgramName() const { return program_name_; }
    
    // Return Name given encode and index of the Name in input Tree
    Name getInputName( const size_t & index );

    size_t getInputCount() const { return input_count_; }

    void attach_input( uint32_t input_index, uint32_t mem_index );

    template<typename T>
    uint32_t get_i32( uint32_t mem_index, uint32_t ofst );
    
    void attach_output( uint32_t output_index, uint32_t mem_index, uint32_t output_type ); 

    void attach_output_child ( uint32_t parent_mem_index, uint32_t child_index, uint32_t child_mem_index, uint32_t output_type );

    template<typename T>
    uint32_t store_i32( uint32_t mem_index, uint32_t content );

    void set_encode( uint32_t mem_index, uint32_t encode_index );

    void add_path( uint32_t mem_index, uint32_t path );
};
    
