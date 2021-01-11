#include <string>
#include <map>

class Invocation {
  private:
    // Name of the program
    std::string program_name_;

    // Map from input name to blob name
    std::map<std::string, std::string> input_to_blob_;

    // Map from output name to blob name
    std::map<std::string, std::string> output_to_blob_;

    // Corresponding memory instance
    uint8_t *mem;
};
    
