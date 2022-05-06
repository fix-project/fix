#include "initcomposer.hh"

#include <sstream>

#include "src/c-writer.h"
#include "src/error.h"

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
  InitComposer( const string& wasm_name, Module* module, Errors* errors, wasminspector::WasmInspector* inspector )
    : current_module_( module )
    , errors_( errors )
    , wasm_name_( wasm_name )
    , state_info_type_name_( MangleStateInfoTypeName( wasm_name ) )
    , module_prefix_( MangleName( wasm_name ) + "_" )
    , result_()
    , inspector_( inspector )
  {}

  InitComposer( const InitComposer& ) = default;
  InitComposer& operator=( const InitComposer& ) = default;

  string compose_header();

private:
  wabt::Module* current_module_;
  wabt::Errors* errors_;
  string wasm_name_;
  string state_info_type_name_;
  string module_prefix_;
  ostringstream result_;
  wasminspector::WasmInspector* inspector_;

  void write_attach_tree();
  void write_attach_blob();
  void write_detach_mem();
  void write_detach_table();
  void write_freeze_blob();
  void write_freeze_tree();
  void write_init_read_only_mem_table();
};

void InitComposer::write_attach_tree()
{
  auto it = inspector_->GetImportedFunctions().find( "attach_tree" );
  if ( it == inspector_->GetImportedFunctions().end() )
    return;

  result_ << "extern void fixpoint_attach_tree(__m256i, wasm_rt_externref_table_t*);" << endl;
  auto ro_tables = inspector_->GetExportedROTables();
  for ( uint32_t idx : ro_tables ) {
    result_ << "void attach_tree_" << idx << "(" << state_info_type_name_
            << "* module_instance, __m256i ro_handle) {" << endl;
    result_ << "  wasm_rt_externref_table_t* ro_table = " << module_prefix_ << "Z_ro_table_" << idx
            << "(module_instance);" << endl;
    result_ << "  fixpoint_attach_tree(ro_handle, ro_table);" << endl;
    result_ << "}\n" << endl;
  }

  result_ << "void " << module_prefix_
          << "Z_env_Z_attach_tree(struct Z_env_module_instance_t* env_module_instance, __m256i ro_handle, uint32_t "
             "ro_table_num) {"
          << endl;
  for ( uint32_t idx : ro_tables ) {
    result_ << "  if (ro_table_num == " << idx << ") {" << endl;
    result_ << "    attach_tree_" << idx << "((" << state_info_type_name_ << "*)env_module_instance, ro_handle);"
            << endl;
    result_ << "    return;" << endl;
    result_ << "  }" << endl;
  }
  result_ << "  wasm_rt_trap(WASM_RT_TRAP_OOB);" << endl;
  result_ << "}" << endl;
  result_ << endl;
}

void InitComposer::write_attach_blob()
{
  auto it = inspector_->GetImportedFunctions().find( "attach_blob" );
  if ( it == inspector_->GetImportedFunctions().end() )
    return;

  auto ro_mems = inspector_->GetExportedROMems();
  result_ << "extern void fixpoint_attach_blob(__m256i, wasm_rt_memory_t*);" << endl;
  for ( uint32_t idx : ro_mems ) {
    result_ << "void attach_blob_" << idx << "(" << state_info_type_name_
            << "* module_instance, __m256i ro_handle) {" << endl;
    result_ << "  wasm_rt_memory_t* ro_mem = " << module_prefix_ << "Z_ro_mem_" << idx << "(module_instance);"
            << endl;
    result_ << "  fixpoint_attach_blob(ro_handle, ro_mem);" << endl;
    result_ << "}\n" << endl;
  }

  result_ << "void " << module_prefix_
          << "Z_env_Z_attach_blob(struct Z_env_module_instance_t* env_module_instance, __m256i ro_handle, "
             "uint32_t ro_mem_num) {"
          << endl;
  for ( uint32_t idx : ro_mems ) {
    result_ << "  if (ro_mem_num == " << idx << ") {" << endl;
    result_ << "    attach_blob_" << idx << "((" << state_info_type_name_ << "*)env_module_instance, ro_handle);"
            << endl;
    result_ << "    return;" << endl;
    result_ << "  }" << endl;
  }
  result_ << "  wasm_rt_trap(WASM_RT_TRAP_OOB);" << endl;
  result_ << "}" << endl;
  result_ << endl;
}

void InitComposer::write_detach_mem()
{
  auto it = inspector_->GetImportedFunctions().find( "detach_mem" );
  if ( it == inspector_->GetImportedFunctions().end() )
    return;

  auto rw_mems = inspector_->GetExportedRWMems();
  result_ << "extern __m256i fixpoint_detach_mem( wasm_rt_memory_t* );" << endl;
  for ( uint32_t idx : rw_mems ) {
    result_ << "__m256i detach_mem_" << idx << "(" << state_info_type_name_ << "* module_instance) {" << endl;
    result_ << "  wasm_rt_memory_t* rw_mem = " << module_prefix_ << "Z_rw_mem_" << idx << "(module_instance);"
            << endl;
    result_ << "  return fixpoint_detach_mem(rw_mem);" << endl;
    result_ << "}\n" << endl;
  }

  result_ << "__m256i " << module_prefix_
          << "Z_env_Z_detach_mem(struct Z_env_module_instance_t* env_module_instance, uint32_t rw_mem_num) {"
          << endl;
  for ( uint32_t idx : rw_mems ) {
    result_ << "  if (rw_mem_num == " << idx << ") {" << endl;
    result_ << "    return detach_mem_" << idx << "((" << state_info_type_name_ << "*)env_module_instance);"
            << endl;
    result_ << "  }" << endl;
  }
  result_ << "  wasm_rt_trap(WASM_RT_TRAP_OOB);" << endl;
  result_ << "}" << endl;
  result_ << endl;
}

void InitComposer::write_detach_table()
{
  auto it = inspector_->GetImportedFunctions().find( "detach_table" );
  if ( it == inspector_->GetImportedFunctions().end() )
    return;

  auto rw_tables = inspector_->GetExportedRWTables();
  result_ << "extern __m256i fixpoint_detach_table( wasm_rt_externref_table_t* );" << endl;
  for ( auto rw_table : rw_tables ) {
    result_ << "__m256i detach_table_" << rw_table << "(" << state_info_type_name_ << "* module_instance) {"
            << endl;
    result_ << "  wasm_rt_externref_table_t* rw_table = " << module_prefix_ << "Z_rw_table_" << rw_table
            << "(module_instance);" << endl;
    result_ << "  return fixpoint_detach_table(rw_table);" << endl;
    result_ << "}\n" << endl;
  }

  result_ << "__m256i " << module_prefix_
          << "Z_env_Z_detach_table(struct Z_env_module_instance_t* env_module_instance, uint32_t rw_table_num) {"
          << endl;
  for ( auto rw_table : rw_tables ) {
    result_ << "  if (rw_table_num == " << rw_table << ") {" << endl;
    result_ << "    return detach_table_" << rw_table << "((" << state_info_type_name_ << "*)env_module_instance);"
            << endl;
    result_ << "  }" << endl;
  }
  result_ << "  wasm_rt_trap(WASM_RT_TRAP_OOB);" << endl;
  result_ << "}" << endl;
  result_ << endl;
}

void InitComposer::write_freeze_blob()
{
  auto it = inspector_->GetImportedFunctions().find( "freeze_blob" );
  if ( it == inspector_->GetImportedFunctions().end() )
    return;

  result_ << "extern __m256i fixpoint_freeze_blob( __m256i, uint32_t );" << endl;
  result_ << "__m256i " << module_prefix_
          << "Z_env_Z_freeze_blob(struct Z_env_module_instance_t* env_module_instance, __m256i rw_handle, "
             "uint32_t size) {"
          << endl;
  result_ << "  return fixpoint_freeze_blob(rw_handle, size);" << endl;
  result_ << "}" << endl;
  result_ << endl;
}

void InitComposer::write_freeze_tree()
{
  auto it = inspector_->GetImportedFunctions().find( "freeze_tree" );
  if ( it == inspector_->GetImportedFunctions().end() )
    return;

  result_ << "extern __m256i fixpoint_freeze_tree( __m256i, uint32_t );" << endl;
  result_ << "__m256i " << module_prefix_
          << "Z_env_Z_freeze_tree(struct Z_env_module_instance_t* env_module_instance, __m256i rw_handle, uint32_t "
             "size) {"
          << endl;
  result_ << "  return fixpoint_freeze_tree(rw_handle, size);" << endl;
  result_ << "}" << endl;
  result_ << endl;
}

void InitComposer::write_init_read_only_mem_table()
{
  result_ << "void init_mems(" << state_info_type_name_ << "* module_instance) {" << endl;
  for ( const auto& ro_mem : inspector_->GetExportedROMems() ) {
    result_ << "  " << module_prefix_ << "Z_ro_mem_" << ro_mem << "(module_instance)->read_only = true;" << endl;
  }
  result_ << "  return;" << endl;
  result_ << "}" << endl;
  result_ << endl;

  result_ << "void init_tables(" << state_info_type_name_ << "* module_instance) {" << endl;
  for ( const auto& ro_table : inspector_->GetExportedROTables() ) {
    result_ << "  " << module_prefix_ << "Z_ro_table_" << ro_table << "(module_instance)->read_only = true;"
            << endl;
  }
  result_ << "  return;" << endl;
  result_ << "}" << endl;
  result_ << endl;
}

string InitComposer::compose_header()
{
  result_ = ostringstream();
  result_ << "#include <immintrin.h>" << endl;
  result_ << "#include \"" << wasm_name_ << "_fixpoint.h\"" << endl;
  result_ << endl;

  write_init_read_only_mem_table();
  write_attach_tree();
  write_attach_blob();
  write_detach_mem();
  write_detach_table();
  write_freeze_blob();
  write_freeze_tree();

  result_ << "extern void* fixpoint_init_module_instance(size_t);" << endl;
  result_ << "void* initProgram() {" << endl;
  result_ << "  " << state_info_type_name_ << "* instance = (" << state_info_type_name_
          << "*)fixpoint_init_module_instance(sizeof(" << state_info_type_name_ << "));" << endl;
  result_ << "  " << module_prefix_ << "init_module();" << endl;
  result_ << "  " << module_prefix_ << "init(instance, (struct Z_env_module_instance_t*)instance);" << endl;
  result_ << "  init_mems(instance);" << endl;
  result_ << "  init_tables(instance);" << endl;
  result_ << "  return (void*)instance;" << endl;
  result_ << "}" << endl;

  return result_.str();
}

string compose_header( string wasm_name, Module* module, Errors* error, wasminspector::WasmInspector* inspector )
{
  InitComposer composer( wasm_name, module, error, inspector );
  return composer.compose_header();
}
}
