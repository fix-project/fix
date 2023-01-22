#include <string>

void llvm_init();

std::string c_to_elf( const std::string_view c_content,
                      const std::string_view h_content,
                      const std::string_view fixpoint_header,
                      const std::string_view wasm_rt_content );
