#include <string>

#include "wabt/error.h"
#include "wabt/ir.h"

#include "wasminspector.hh"

namespace initcomposer {
std::string compose_header( std::string wasm_name,
                            wabt::Module* module,
                            wabt::Errors* error,
                            wasminspector::WasmInspector* inspector );
}
