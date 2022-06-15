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

struct InstanceCleanUp
{
  uint64_t address;

  void operator()( char* ptr ) const
  {
    void ( *cleanup_func )( void* );
    cleanup_func = reinterpret_cast<void ( * )( void* )>( address );
    cleanup_func( ptr );
    free( ptr );
  }
};

class Program
{
private:
  // Code and data section of the program
  std::shared_ptr<char> code_;
  // Entry point of init function
  uint64_t init_entry_;
  // Entry point of main function
  uint64_t main_entry_;
  // Entry point of clean up function
  uint64_t cleanup_entry_;
  // size of instance
  size_t instance_context_size_;
  // number of init instances
  size_t init_instances_;
  // Array of instances
  char* instances_;
  // Index of the next available instance
  size_t next_instance_;

  struct Context
  {
    size_t memory_usage;
  };

public:
  Program( std::shared_ptr<char> code,
           uint64_t init_entry,
           uint64_t main_entry,
           uint64_t cleanup_entry,
           uint64_t instance_size_entry,
           uint64_t init_module_entry,
           size_t init_instances )
    : code_( code )
    , init_entry_( init_entry )
    , main_entry_( main_entry )
    , cleanup_entry_( cleanup_entry )
    , instance_context_size_( 0 )
    , init_instances_( init_instances )
    , instances_()
    , next_instance_( 0 )
  {
    GlobalScopeTimer<Timer::Category::Populating> record_timer;
    size_t ( *size_func )( void );
    size_func = reinterpret_cast<size_t ( * )( void )>( code_.get() + instance_size_entry );
    size_t instance_size = size_func();
    instance_context_size_ = instance_size + sizeof( Context );
    if ( instance_context_size_ % alignof( __m256i ) != 0 ) {
      instance_context_size_ = ( instance_context_size_ / alignof( __m256i ) + 1 ) * alignof( __m256i );
    }

    void ( *init_module_func )( void );
    init_module_func = reinterpret_cast<void ( * )( void )>( code_.get() + init_module_entry );
    init_module_func();

    void ( *init_func )( void* );
    init_func = reinterpret_cast<void ( * )( void* )>( code_.get() + init_entry_ );
    instances_
      = static_cast<char*>( aligned_alloc( alignof( __m256i ), instance_context_size_ * init_instances_ ) );
    for ( size_t i = 0; i < init_instances_; i++ ) {
      init_func( instances_ + i * instance_context_size_ );
    }
  }

  void populate_instance_and_context( string_span instance ) const
  {
    void ( *init_func )( void* );
    init_func = reinterpret_cast<void ( * )( void* )>( code_.get() + init_entry_ );
    init_func( instance.mutable_data() );
  }

  size_t get_instance_and_context_size() const { return instance_context_size_; }

  __m256i execute( Name encode_name )
  {
    __m256i ( *main_func )( void*, __m256i );
    main_func = reinterpret_cast<__m256i ( * )( void*, __m256i )>( code_.get() + main_entry_ );
    void* inst = instances_ + next_instance_ * instance_context_size_;

    // void ( *init_func )( void* );
    // init_func = reinterpret_cast<void ( * )( void* )>( code_.get() + init_entry_ );
    // init_func( inst );

    __m256i result = main_func( inst, encode_name );
    next_instance_++;
    return result;
  }

  Program( const Program& ) = delete;
  Program& operator=( const Program& ) = delete;

  Program( Program&& other )
    : code_( other.code_ )
    , init_entry_( other.init_entry_ )
    , main_entry_( other.main_entry_ )
    , cleanup_entry_( other.cleanup_entry_ )
    , instance_context_size_( other.instance_context_size_ )
    , init_instances_( other.init_instances_ )
    , instances_( other.instances_ )
    , next_instance_( other.next_instance_ )
  {
    if ( this != &other ) {
      other.instances_ = nullptr;
    }
  }

  Program& operator=( Program&& other )
  {
    code_ = other.code_;
    init_entry_ = other.init_entry_;
    main_entry_ = other.main_entry_;
    cleanup_entry_ = other.cleanup_entry_;
    instance_context_size_ = other.instance_context_size_;
    instances_ = other.instances_;
    next_instance_ = other.next_instance_;

    if ( this != &other ) {
      other.instances_ = nullptr;
    }

    return *this;
  }

  ~Program()
  {
    if ( instances_ ) {
      void ( *cleanup_func )( void* );
      cleanup_func = reinterpret_cast<void ( * )( void* )>( code_.get() + cleanup_entry_ );
      for ( size_t i = 0; i < init_instances_; i++ ) {
        cleanup_func( instances_ + i * instance_context_size_ );
      }
      free( instances_ );
    }
  }
};
