#include <string>

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

typedef struct input_entry {
  char[256] symbol;
} InputEntry;

typedef struct output_entry {
  char[256] symbol;
} OutputEntry;

typedef struct p_mem {
  // Code section of the program
  void *code;
  // Data section of the program
  void *data;
  // Entry point of the program
  void *entry_point;
} ProgramMem;

class Program {
  private:
    // Name of the program
    std::string name_;
    
    // List of named input symbols
    span_view<InputEntry> inputs_;
    // List of named output symbols
    span_view<OutputEntry> outputs_;

    // memory instance of the program
    ProgramMem memory_instance_;

  public:
    // Default constructor
    Program() {};
    Program( string name ) : name_( name );
}
