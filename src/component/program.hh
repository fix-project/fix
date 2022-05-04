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
#include "timing_helper.hh"

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

public:
  Program( std::string name,
           std::shared_ptr<char> code,
           uint64_t init_entry,
           uint64_t main_entry,
           uint64_t cleanup_entry )
    : name_( name )
    , code_( code )
    , init_entry_( init_entry )
    , main_entry_( main_entry )
    , cleanup_entry_( cleanup_entry )
  {}

  __m256i execute( Name encode_name ) const
  {
    void* ( *init_func )( void );
    init_func = reinterpret_cast<void* (*)( void )>( code_.get() + init_entry_ );
    void* instance = init_func();

    __m256i ( *main_func )( void*, __m256i );
    main_func = reinterpret_cast<__m256i ( * )( void*, __m256i )>( code_.get() + main_entry_ );

    __m256i output;

    {
#if !TIME_FIXPOINT_API
      RecordScopeTimer<Timer::Category::Nonblock> record_timer { _fixpoint_apply };
#endif
      output = main_func( instance, encode_name );
    }

    void* ( *cleanup_func )( void* );
    cleanup_func = reinterpret_cast<void* (*)( void* )>( code_.get() + cleanup_entry_ );
    cleanup_func( instance );
    free( instance );
    return output;
  }
};
