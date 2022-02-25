#include "initcomposer.hh"

#include <sstream>

#include "src/c-writer.h"
using namespace std;

std::string compose_init( const std::string& wasm_name, const std::string& h_content )
{
  std::ostringstream result;

  // #include "wasm_name.h"
  result << "#include \"" << wasm_name << ".h\"\n";

  // get name of module_instance_t from header
  istringstream lines( h_content );
  string line;
  string module_instance_type;
  while ( getline( lines, line )  ) {
    if ( line.find( "_module_instance_t" ) != string::npos ) {
      module_instance_type = line.substr(2, line.length() - 1);
      break;
    }
  }

  // void executeProgram() {
  //   module_instance_t instance;
  //   init(&instance);
  //   start(&instance);
  // }
  result << "void executeProgram() {" << endl;
  result << "  " << module_instance_type << " instance;" << endl;
  result << "  init(&instance);" << endl;
  result << "  start(&instance);" << endl;
  result << "}" << endl;

  return result.str();
}
