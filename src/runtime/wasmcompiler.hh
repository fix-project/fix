#include <string>
#include <string_view>
#include <utility>

#include "src/stream.h"

namespace wasmcompiler {
std::tuple<std::string, std::string, std::string> wasm_to_c( const std::string_view wasm_content );
}
