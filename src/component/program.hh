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
        inputs_( std::move( inputs ) ),
        outputs_( std::move( outputs ) ),
        code_( code ),
        init_entry_( init_entry ),
        main_entry_( main_entry ),
        mem_loc_( mem_loc )
    {}

    void execute ()
    { 
      // int register rax asm("rax") = init_entry_;
      asm("call *%0"
           :
           : "r" ( (uint64_t)( code_.get() + init_entry_ ) ) );
      

      int register esi asm("esi") = 13;
      int register edi asm("edi") = 20;
      std::cout << "esi is " << esi << " edi is " << edi << std::endl;
      asm("call *%0"
           :
           : "r" ( (uint64_t)( code_.get() + main_entry_ ) ) );
    }

    const std::vector<std::string> & getInputSymbols() const { return inputs_; } 
    const std::vector<std::string> & getOutputSymbols() const { return outputs_; } 
    char * getMemLoc() const { return code_.get() + mem_loc_; }
};
