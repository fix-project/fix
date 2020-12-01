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
    // Content of the program
    std::string content_;
    // Corresponding file descriptor
    int fd_;
    // File descriptor wrapper
    FileDescriptor fd_wrapper_;
    
    uint64_t num_input_entry_;
    uint64_t num_output_entry_;
    // List of named input symbols
    span_view<InputEntry> inputs_;
    // List of named output symbols
    span_view<OutputEntry> outputs_;

    // Path of the wasm source code
    const char* wasm_path_;
    // Whether the wasm source code has been cached
    bool wasm_cached_;

    // Pointer to memory instance of the program
    ProgramMem * memory_instance_;
    // Whether the program is in memory
    bool memory_cached_;

  public:
    // Default constructor
    Program() {};
    Program( string name ) : name_( name );

    // Load the program into memory
    void load_to_memory();

    // Remove the program from memory
    void clear_from_memory();

    void parse( Parser& p );
}
