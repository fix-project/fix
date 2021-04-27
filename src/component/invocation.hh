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
#define NAME 3

#define MEM_CAPACITY 256

using InputMem = Name;

using InProgressBlob = std::string;

using InProgressTree = size_t;

struct InProgressThunk 
{
  std::vector<size_t> encode_path_;
  std::vector<size_t> path_;

  InProgressThunk()
  : encode_path_(),
    path_()
  {}
};

struct OutputTemp
{
  std::vector<size_t> path_;
  int content_type_; 
  std::variant<InProgressBlob, InProgressTree, InProgressThunk, Name> content_;

  OutputTemp( size_t output_index, int output_type ) 
  : path_( { output_index } ),
    content_type_( output_type ),
    content_()
  {
    switch ( output_type )
    {
      case BLOB :
        content_.emplace<InProgressBlob>();
        break;

      case TREE :
        content_.emplace<InProgressTree>( 0 );
        break;

      case THUNK :
        content_.emplace<InProgressThunk>();
        break;
    } 
  }

  OutputTemp( std::vector<size_t> path, size_t output_index, int output_type )
  : path_( path ),
    content_type_( output_type ),
    content_()
  {
    path_.push_back( output_index );
    switch ( output_type )
    {
      case BLOB :
        content_.emplace<InProgressBlob>();
        break;

      case TREE :
        content_.emplace<InProgressTree>( 0 );
        break;

      case THUNK :
        content_.emplace<InProgressThunk>();
        break;
    }
  } 

  OutputTemp( size_t output_index, Name name )
  : path_( { output_index } ),
    content_type_( NAME ),
    content_( name )
  {}

  OutputTemp( std::vector<size_t> path, size_t output_index, Name name )
  : path_( path ),
    content_type_( NAME ),
    content_( name )
  {
    path_.push_back( output_index );
  }
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
    InputMem input_mems [MEM_CAPACITY];

    // Map from id to output mems
    OutputTemp* output_mems [MEM_CAPACITY];

    std::vector<OutputTemp> outputs;

    // vector of the number of strict inputs/lazy inputs
    std::vector<size_t> num_inputs_;

    // The number of all inputs
    size_t input_count_;

    // The number of depth 0 outputs
    size_t output_count_;

  public:
    Invocation( std::string program_name, Name encode_name, wasm_rt_memory_t *mem ) 
      : program_name_( program_name ),
        encode_name_( encode_name ),
        mem_( mem ),
        input_mems(),
        output_mems(),
        outputs(),
        num_inputs_(),
        input_count_( 0 ),
        output_count_( 0 )
    {}

    Invocation( Name encode_name )
      : program_name_( "" ),
        encode_name_( encode_name ),
        mem_( 0 ),
        input_mems(),
        output_mems(),
        outputs(),
        num_inputs_(),
        input_count_( 0 ),
        output_count_( 0 )
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
    void store_i32( uint32_t mem_index, uint32_t content );

    void set_encode( uint32_t mem_index, uint32_t encode_index );

    void add_path( uint32_t mem_index, uint32_t path );

    void move_lazy_input( uint32_t mem_index, uint32_t child_index, uint32_t lazy_input_index );

    void add_to_storage();

    uint32_t get_int( uint32_t mem_index, uint32_t ofst );
    void store_int( uint32_t mem_index, uint32_t content );
};
    
