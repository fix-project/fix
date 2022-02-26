#include "initcomposer.hh"

#include <sstream>

#include "src/c-writer.h"

using namespace std;

namespace initcomposer {
string MangleName( string_view name )
{
  const char kPrefix = 'Z';
  std::string result = "Z_";

  if ( !name.empty() ) {
    for ( char c : name ) {
      if ( ( isalnum( c ) && c != kPrefix ) || c == '_' ) {
        result += c;
      } else {
        result += kPrefix;
        result += wabt::StringPrintf( "%02X", static_cast<uint8_t>( c ) );
      }
    }
  }

  return result;
}

string MangleStateInfoTypeName( const string& wasm_name )
{
  return MangleName( wasm_name ) + "_module_instance_t";
}

string compose_init( const string& wasm_name )
{
  ostringstream result;

  // #include "wasm_name.h"
  result << "#include \"" << wasm_name << ".h\"" << endl;
  result << endl;

  //#include <stdint.h>
  //#include <stddef.h>
  result << "#include <stdint.h>" << endl;
  result << "#include <stddef.h>" << endl;
  result << endl;

  // get name of module_instance_t
  string module_instance_type = MangleStateInfoTypeName( wasm_name );

  // static size_t memory_offset;
  result << "static size_t memory_offset;" << endl;
  result << endl;

  // void initializeOffset() {
  //   memory_offset = offsetof(_module_instance_t, w2c_M0);
  // }
  result << "void initializeOffset() {" << endl;
  result << "  memory_offset = offsetof(" << module_instance_type << ", w2c_M0);" << endl;
  result << "}" << endl;
  result << endl;

  // void executeProgram() {
  //   module_instance_t instance;
  //   init(&instance);
  //   start(&instance);
  // }
  result << "void executeProgram() {" << endl;
  result << "  " << module_instance_type << " instance;" << endl;
  result << "  init(&instance);" << endl;
  result << "  _fixpoint_apply(&instance);" << endl;
  result << "}" << endl;

  return result.str();
}
}
