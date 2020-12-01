using namespace std;

/**
 * Structure of Encode:
 * --------------------------------
 *  header:name of program
 *         number of input entries
 *         number of output entries
 * --------------------------------
 *        Input entries
 * --------------------------------
 *        Output entries
 * --------------------------------
 *        Wasm module
 * --------------------------------
 */

typedef struct encode_header {
  char[32] program_name;
  uint64_t num_input_entry;
  uint64_t num_output_entry;
} ProgFileHdr;

typedef struct input_entry {
  char[256] symbol;
  char[32] blob_name;
} InputEntry;

typedef struct output_entry {
  char[256] symbol;
  char[32] blob_name;
} OutputEntry;

class Encode {
  private:
    // Name of the encode 
    char name[32];
    // Content of the program
    byte *content;
    // List of named input symbols
    InputEntry * inputs;
    // List of named output symbols
    OutputEntry * outputs;
    
    bool encode_completed;
  public:
    // Default constructor
    Encode() {};

    // Destructor
    ~Encode();

    void execute();
}
