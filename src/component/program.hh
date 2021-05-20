#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "parser.hh"
#include "spans.hh"
#include "wasm-rt.h"

/**
 * Structure of program files:
 * --------------------------------
 *  header:number of input entries
 *         number of output entries
 * --------------------------------
 *        Input entries
 * --------------------------------
 *        Output entries
 * --------------------------------
 *        Wasm module
 * --------------------------------
 */

class Program {
  private:
    // Name of the program
    std::string name_;
    // List of dependency
    std::vector<std::string> deps_;
    
    // List of named input symbols
    std::vector<std::string> inputs_;
    // List of named output symbols
    std::vector<std::string> outputs_;

    // Code and data section of the program
    std::shared_ptr<char> code_;
    // Entry point of init function
    uint64_t init_entry_;
    // Entry point of main function
    uint64_t main_entry_;
    // Location of memory variable;
    uint64_t mem_loc_;

  public:
    Program( std::string name, std::vector<std::string> && inputs, std::vector<std::string> && outputs,
             std::shared_ptr<char> code, uint64_t init_entry, uint64_t main_entry, uint64_t mem_loc ) 
      : name_( name ),
        deps_(),
        inputs_( std::move( inputs ) ),
        outputs_( std::move( outputs ) ),
        code_( code ),
        init_entry_( init_entry ),
        main_entry_( main_entry ),
        mem_loc_( mem_loc )
    {}

    void execute () const
    { 
      void (*init)();
      init = reinterpret_cast<void (*) ()>(code_.get() + init_entry_);
      init();

      void (*main_func)();
      main_func = reinterpret_cast<void (*) ()>(code_.get() + main_entry_);
      main_func();
    }

    const std::vector<std::string> & getInputSymbols() const { return inputs_; } 
    const std::vector<std::string> & getOutputSymbols() const { return outputs_; } 
    char * getMemLoc() const { return code_.get() + mem_loc_; }

    void setDeps( const std::vector<std::string> & deps )
    {
      deps_ = deps;
    }
    const std::vector<std::string> & getDeps() const { return deps_; }
};
