#include "initcomposer.hh"

#include <sstream>

#include "src/c-writer.h"
#include "src/error.h"
#include "wasminspector.hh"

using namespace std;
using namespace wabt;

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

class InitComposer
{
public:
  InitComposer( const string& wasm_name, Module* module, Errors* errors )
    : current_module_( module )
    , errors_( errors )
    , wasm_name_( wasm_name )
    , state_info_type_name_( MangleStateInfoTypeName( wasm_name ) )
    , result_()
    , inspector_( module, errors )
  {}

  InitComposer( const InitComposer& ) = default;
  InitComposer& operator=( const InitComposer& ) = default;

  string compose_init();
  string compose_header();

private:
  wabt::Module* current_module_;
  wabt::Errors* errors_;
  string wasm_name_;
  string state_info_type_name_;
  ostringstream result_;
  wasminspector::WasmInspector inspector_;

  void write_get_tree_entry();
  void write_attach_blob();
  void write_detach_mem();
  void write_freeze_blob();
  void write_designate_output();
};

void InitComposer::write_get_tree_entry()
{
  auto it = inspector_.GetImportedFunctions().find( "get_tree_entry" );
  if ( it != inspector_.GetImportedFunctions().end() ) {
    result_ << "extern void fixpoint_get_tree_entry( void*, uint32_t, uint32_t, uint32_t );" << endl;
    result_ << "void get_tree_entry(" << state_info_type_name_
            << "* module_instance, uint32_t src_ro_handle, uint32_t entry_num, uint32_t target_ro_handle) {"
            << endl;
    result_ << "  fixpoint_get_tree_entry(module_instance, src_ro_handle, entry_num, target_ro_handle);" << endl;
    result_ << "}\n" << endl;
  }
}

void InitComposer::write_attach_blob()
{
  auto ro_mems = inspector_.GetExportedRO();
  auto it = inspector_.GetImportedFunctions().find( "attach_blob" );
  if ( ro_mems.size() != 0 && it != inspector_.GetImportedFunctions().end() ) {
    result_ << "extern void fixpoint_attach_blob( void*, wasm_rt_memory_t*, uint32_t );" << endl;
    for ( uint32_t idx : ro_mems ) {
      result_ << "void attach_blob_" << idx << "(" << state_info_type_name_
              << "* module_instance, uint32_t ro_handle) {" << endl;
      result_ << "  wasm_rt_memory_t* ro_mem = Z_ro_mem_" << idx << "(module_instance);" << endl;
      result_ << "  fixpoint_attach_blob(module_instance, ro_mem, ro_handle);" << endl;
      result_ << "}\n" << endl;
    }
  }

  if ( it != inspector_.GetImportedFunctions().end() ) {
    result_ << "void attach_blob(" << state_info_type_name_
            << "* module_instance, uint32_t ro_mem_num, uint32_t ro_handle) {" << endl;
    for ( uint32_t idx : ro_mems ) {
      result_ << "  if (ro_mem_num == " << idx << ") {" << endl;
      result_ << "    attach_blob_" << idx << "(module_instance, ro_handle);" << endl;
      result_ << "    return;" << endl;
      result_ << "  }" << endl;
    }
    result_ << "  wasm_rt_trap(WASM_RT_TRAP_OOB);" << endl;
    result_ << "}\n" << endl;
  }
}

void InitComposer::write_detach_mem()
{
  auto rw_mems = inspector_.GetExportedRW();
  auto it = inspector_.GetImportedFunctions().find( "detach_mem" );
  if ( rw_mems.size() != 0 && it != inspector_.GetImportedFunctions().end() ) {
    result_ << "extern void fixpoint_detach_mem( void*, wasm_rt_memory_t*, uint32_t );" << endl;
    for ( uint32_t idx : rw_mems ) {
      result_ << "void detach_mem_" << idx << "(" << state_info_type_name_
              << "* module_instance, uint32_t rw_handle) {" << endl;
      result_ << "  wasm_rt_memory_t* rw_mem = Z_rw_mem_" << idx << "(module_instance);" << endl;
      result_ << "  fixpoint_detach_mem(module_instance, rw_mem, rw_handle);" << endl;
      result_ << "}\n" << endl;
    }
  }

  if ( it != inspector_.GetImportedFunctions().end() ) {
    result_ << "void detach_mem(" << state_info_type_name_
            << "* module_instance, uint32_t rw_mem_num, uint32_t rw_handle) {" << endl;
    for ( uint32_t idx : rw_mems ) {
      result_ << "  if (rw_mem_num == " << idx << ") {" << endl;
      result_ << "    detach_mem_" << idx << "(module_instance, rw_handle);" << endl;
      result_ << "    return;" << endl;
      result_ << "  }" << endl;
    }
    result_ << "  wasm_rt_trap(WASM_RT_TRAP_OOB);" << endl;
    result_ << "}\n" << endl;
  }
}

void InitComposer::write_freeze_blob()
{
  auto it = inspector_.GetImportedFunctions().find( "freeze_blob" );
  if ( it != inspector_.GetImportedFunctions().end() ) {
    result_ << "extern void fixpoint_freeze_blob( void*, uint32_t, uint32_t, uint32_t );" << endl;
    result_ << "void freeze_blob(" << state_info_type_name_
            << "* module_instance, uint32_t rw_handle, uint32_t size, uint32_t ro_handle) {" << endl;
    result_ << "  fixpoint_freeze_blob(module_instance, rw_handle, size, ro_handle);" << endl;
    result_ << "}\n" << endl;
  }
}

void InitComposer::write_designate_output()
{
  auto it = inspector_.GetImportedFunctions().find( "designate_output" );
  if ( it != inspector_.GetImportedFunctions().end() ) {
    result_ << "extern void fixpoint_designate_output( void*, uint32_t );" << endl;
    result_ << "void designate_output(" << state_info_type_name_ << "* module_instance, uint32_t ro_handle) {"
            << endl;
    result_ << "  fixpoint_designate_output(module_instance, ro_handle);" << endl;
    result_ << "}\n" << endl;
  }
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
  result << "  Z__fixpoint_applyZ_vv(&instance);" << endl;
  result << "}" << endl;

  return result.str();
}
}
