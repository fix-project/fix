#include <string>
#include <memory>

#include "parser.hh"
#include "spans.hh"

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
    span_view<std::string> inputs_;
    // List of named output symbols
    span_view<std::string> outputs_;

    // Code and data section of the program
    std::shared_ptr<const char> code_;
    // Entry point of init function
    uint64_t init_entry_;
    // Entry point of main function
    uint64_t main_entry_;

  public:
    Program( std::string name, span_view<std::string> inputs, span_view<std::string> outputs,
             std::shared_ptr<const char> code, uint64_t init_entry, uint64_t main_entry ) 
      : name_( name ),
        inputs_( inputs ),
        outputs_( outputs ),
        code_( code ),
        init_entry_( init_entry ),
        main_entry_( main_entry ) {}
};
