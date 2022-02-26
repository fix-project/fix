#include <string>

std::string c_to_elf( const std::string& wasm_name,
                      std::string& c_content,
                      const std::string& h_content,
                      const std::string& init,
                      const std::string& wasm_rt_content = "" );
