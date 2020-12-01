using namespace std;

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

typedef struct program_file_header {
  uint64_t num_input_entry;
  uint64_t num_output_entry;
} ProgFileHdr;

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
} ProogramMem;

class Program {
  private:
    // Name of the program
    char name[32];
    // Content of the program
    byte *content;
    // List of named input symbols
    InputEntry * inputs;
    // List of named output symbols
    OutputEntry * outputs;
     
    // Path of the wasm source code
    const char* wasm_path;
    // Whether the wasm source code has been cached
    bool wasm_cached;

    // Pointer to memory instance of the program
    ProgramMem * memory_instance;
    // Whether the program is in memory
    bool memory_cached;

  public:
    // Default constructor
    Program() {};
    Program(char name[32]) : name(name) {};

    // Destructor
    ~Program();

    // Load the program into memory
    Program_mem *load_to_memory();

    // Remove the program from memory
    void clear_from_memory();
}
