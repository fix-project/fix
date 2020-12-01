#include <map>
#include "program.hpp"
#include "blob.hpp"
#include "encode.hpp"

using namespace std;

/**
 * TODO: What needs to be persisted?
 * Map from program to compiled .o file 
 */
 
class Runtime {
  private:
    // List of pending executions
    List<Execution> pending_executions;

    // Map from name to content (blob / encoded blob)
    map<char[32], byte *> name_to_blob;
    // Map from name to encode
    map<char[32], byte *> name_to_encode;
    // Map from name to program
    map<char[32], byte *> name_to_program;

  public:
}
