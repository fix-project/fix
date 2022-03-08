#include <string>

#include "src/error.h"
#include "src/ir.h"

namespace initcomposer {
std::string compose_header( std::string wasm_name, wabt::Module* module, wabt::Errors* error );
}
