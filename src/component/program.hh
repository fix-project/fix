#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "name.hh"
#include "parser.hh"
#include "spans.hh"
#include "wasm-rt.h"

#include "timer.hh"

class Program
{
private:
  // Name of the program
  std::string name_;

  // Code and data section of the program
  std::shared_ptr<char> code_;
  // Entry point of init function
  uint64_t init_entry_;
  // Entry point of main function
  uint64_t main_entry_;
  // Entry point of clean up function
  uint64_t cleanup_entry_;
  // size of instance
  size_t instance_size_;

public:
  Program( std::string name,
           std::shared_ptr<char> code,
           uint64_t init_entry,
           uint64_t main_entry,
           uint64_t cleanup_entry,
           uint64_t instance_size_entry,
           uint64_t init_module_entry )
    : name_( name )
    , code_( code )
    , init_entry_( init_entry )
    , main_entry_( main_entry )
    , cleanup_entry_( cleanup_entry )
    , instance_size_( 0 )
  {
    size_t ( *size_func )( void );
    size_func = reinterpret_cast<size_t ( * )( void )>( code_.get() + instance_size_entry );
    instance_size_ = size_func();

    void ( *init_module_func )( void );
    init_module_func = reinterpret_cast<void ( * )( void )>( code_.get() + init_module_entry );
    init_module_func();
  }

  struct Context
  {
    Name return_value;
    size_t memory_usage;
  };

  size_t get_instance_and_context_size() const { return instance_size_ + sizeof( Context ); }

  void populate_instance_and_context( string_span instance ) const
  {
    void ( *init_func )( void* );
    init_func = reinterpret_cast<void ( * )( void* )>( code_.get() + init_entry_ );
    init_func( instance.mutable_data() );

    Context* c = reinterpret_cast<Context*>( instance.mutable_data() + instance_size_ );
    c->memory_usage = 0;
  }

  __m256i execute( Name encode_name, string_span instance ) const
  {
    GlobalScopeTimer<Timer::Category::Execution> record_timer;
    __m256i ( *main_func )( void*, __m256i );
    main_func = reinterpret_cast<__m256i ( * )( void*, __m256i )>( code_.get() + main_entry_ );
    return main_func( instance.mutable_data(), encode_name );
  }

  void cleanup( string_span instance ) const
  {
    void ( *cleanup_func )( void* );
    cleanup_func = reinterpret_cast<void ( * )( void* )>( code_.get() + cleanup_entry_ );
    cleanup_func( instance.mutable_data() );
  }
};
