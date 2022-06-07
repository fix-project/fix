#include <string>

std::string c_to_elf( const std::string& c_content,
                      const std::string& h_content,
                      const std::string& fixpoint_header,
                      const std::string& wasm_rt_content = "" );
