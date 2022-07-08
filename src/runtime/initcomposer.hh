#include <string>

#include "src/error.h"
#include "src/ir.h"

#include "wasminspector.hh"

namespace initcomposer {
std::string compose_header( std::string wasm_name,
                            wabt::Module* module,
                            wabt::Errors* error,
                            wasminspector::WasmInspector* inspector );
}
