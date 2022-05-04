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
  void write_set_ro_handles();
  void write_detach_mem();
  void write_freeze_blob();
  void write_designate_output();
  void write_env_instance();
};

void InitComposer::write_attach_tree()
{
  auto it = inspector_->GetImportedFunctions().find( "attach_tree" );
  if ( it == inspector_->GetImportedFunctions().end() )
    return;

  result_ << "extern void fixpoint_attach_tree(__m256i, wasm_rt_externref_table_t*);" << endl;
  auto ro_tables = inspector_->GetImportedROTables();
  for ( uint32_t idx : ro_tables ) {
    result_ << "void attach_tree_" << idx << "(Z_env_module_instance_t* env_module_instance, __m256i ro_handle) {"
            << endl;
    result_ << "  wasm_rt_externref_table_t* ro_table = &env_module_instance->ro_table_" << idx << ";" << endl;
    result_ << "  fixpoint_attach_tree(ro_handle, ro_table);" << endl;
    result_ << "}\n" << endl;
  }

  result_ << "void " << module_prefix_
          << "Z_env_Z_attach_tree(struct Z_env_module_instance_t* env_module_instance, __m256i ro_handle, uint32_t "
             "ro_table_num) {"
          << endl;
  for ( uint32_t idx : ro_tables ) {
    result_ << "  if (ro_table_num == " << idx << ") {" << endl;
    result_ << "    attach_tree_" << idx << "(env_module_instance, ro_handle);" << endl;
    result_ << "    return;" << endl;
    result_ << "  }" << endl;
  }
  result_ << "  wasm_rt_trap(WASM_RT_TRAP_OOB);" << endl;
  result_ << "}" << endl;
  result_ << endl;
}

void InitComposer::write_set_ro_handles()
{
  result_ << "void " << module_prefix_
          << "Z_env_Z_set_ro_handles(struct Z_env_module_instance_t* env_module_instance, uint32_t offset, __m256i "
             "ro_handle) {"
          << endl;
  result_ << "  wasm_rt_externref_table_t* ro_handles = " << module_prefix_
          << "Z_ro_handles(env_module_instance->module_instance);" << endl;
  result_ << "  if (UNLIKELY(offset >= ro_handles->size)) wasm_rt_trap(WASM_RT_TRAP_OOB);" << endl;
  result_ << "  __m256i original_ro_handle = ro_handles->data[offset];" << endl;
  result_ << "  if (UNLIKELY( ( _mm256_extract_epi8( original_ro_handle, 31 ) & 0x20 ) != 0)) "
             "wasm_rt_trap(WASM_RT_TRAP_UNREACHABLE);"
          << endl;
  result_ << "  ro_handles->data[offset] = ro_handle;" << endl;
  result_ << "  return;" << endl;
  result_ << "}" << endl;
}

void InitComposer::write_attach_blob()
{
  auto ro_mems = inspector_->GetExportedRO();
  auto it = inspector_->GetImportedFunctions().find( "attach_blob" );
  if ( it == inspector_->GetImportedFunctions().end() )
    return;

  result_ << "extern void fixpoint_attach_blob(__m256i, wasm_rt_memory_t*);" << endl;
  for ( uint32_t idx : ro_mems ) {
    result_ << "void attach_blob_" << idx << "(Z_env_module_instance_t"
            << "* env_module_instance, __m256i ro_handle) {" << endl;
    result_ << "  wasm_rt_memory_t* ro_mem = &env_module_instance->ro_mem_" << idx << ";" << endl;
    result_ << "  fixpoint_attach_blob(ro_handle, ro_mem);" << endl;
    result_ << "}\n" << endl;
  }

  result_ << "void " << module_prefix_
          << "Z_env_Z_attach_blob(struct Z_env_module_instance_t* env_module_instance, __m256i ro_handle, "
             "uint32_t ro_mem_num) {"
          << endl;
  for ( uint32_t idx : ro_mems ) {
    result_ << "  if (ro_mem_num == " << idx << ") {" << endl;
    result_ << "    attach_blob_" << idx << "(env_module_instance, ro_handle);" << endl;
    result_ << "    return;" << endl;
    result_ << "  }" << endl;
  }
  result_ << "  wasm_rt_trap(WASM_RT_TRAP_OOB);" << endl;
  result_ << "}" << endl;
  result_ << endl;
}

void InitComposer::write_detach_mem()
{
  auto rw_mems = inspector_->GetExportedRW();
  auto it = inspector_->GetImportedFunctions().find( "detach_mem" );
  if ( it == inspector_->GetImportedFunctions().end() )
    return;

  result_ << "extern __m256i fixpoint_detach_mem( wasm_rt_memory_t* );" << endl;
  for ( uint32_t idx : rw_mems ) {
    result_ << "__m256i detach_mem_" << idx << "(Z_env_module_instance_t"
            << "* env_module_instance) {" << endl;
    result_ << "  wasm_rt_memory_t* rw_mem = &env_module_instance->rw_mem_" << idx << ";" << endl;
    result_ << "  return fixpoint_detach_mem(rw_mem);" << endl;
    result_ << "}\n" << endl;
  }

  result_ << "__m256i " << module_prefix_
          << "Z_env_Z_detach_mem(struct Z_env_module_instance_t* env_module_instance, uint32_t rw_mem_num) {"
          << endl;
  for ( uint32_t idx : rw_mems ) {
    result_ << "  if (rw_mem_num == " << idx << ") {" << endl;
    result_ << "    return detach_mem_" << idx << "(env_module_instance);" << endl;
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

void InitComposer::write_designate_output()
{
  auto it = inspector_->GetImportedFunctions().find( "designate_output" );
  if ( it == inspector_->GetImportedFunctions().end() )
    return;

  result_ << "extern void fixpoint_designate_output( __m256i );" << endl;
  result_ << "void " << module_prefix_
          << "Z_env_Z_designate_output(struct Z_env_module_instance_t* env_module_instance, __m256i ro_handle) {"
          << endl;
  result_ << "  fixpoint_designate_output(ro_handle);" << endl;
  result_ << "}" << endl;
  result_ << endl;
}

void InitComposer::write_env_instance()
{
  result_ << "typedef struct Z_env_module_instance_t {" << endl;
  result_ << "  " << module_prefix_ << "module_instance_t* module_instance;" << endl;

  auto ro_mems = inspector_->GetExportedRO();
  auto rw_mems = inspector_->GetExportedRW();
  auto ro_tables = inspector_->GetImportedROTables();
  auto rw_tables = inspector_->GetImportedRWTables();

  for ( auto ro_mem : ro_mems ) {
    result_ << "  wasm_rt_memory_t ro_mem_" << ro_mem << ";" << endl;
  }
  for ( auto rw_mem : rw_mems ) {
    result_ << "  wasm_rt_memory_t rw_mem_" << rw_mem << ";" << endl;
  }

  for ( auto ro_table : ro_tables ) {
    result_ << "  wasm_rt_externref_table_t ro_table_" << ro_table << ";" << endl;
  }

  for ( auto rw_table : rw_tables ) {
    result_ << "  wasm_rt_externref_table_t rw_table_" << rw_table << ";" << endl;
  }

  result_ << "} Z_env_module_instance_t;" << endl;
  result_ << endl;

  for ( auto ro_mem : ro_mems ) {
    result_ << "wasm_rt_memory_t* Z_env_Z_ro_mem_" << ro_mem << "(Z_env_module_instance_t* env_module_instance) {"
            << endl;
    result_ << "  return &env_module_instance->ro_mem_" << ro_mem << ";" << endl;
    result_ << "}" << endl;
    result_ << endl;
  }

  for ( auto rw_mem : rw_mems ) {
    result_ << "wasm_rt_memory_t* Z_env_Z_rw_mem_" << rw_mem << "(Z_env_module_instance_t* env_module_instance) {"
            << endl;
    result_ << " return &env_module_instance->rw_mem_" << rw_mem << ";" << endl;
    result_ << "}" << endl;
    result_ << endl;
  }

  for ( auto ro_table : ro_tables ) {
    result_ << "wasm_rt_externref_table_t* Z_env_Z_ro_table_" << ro_table
            << "(Z_env_module_instance_t* env_module_instance) {" << endl;
    result_ << "  return &env_module_instance->ro_table_" << ro_table << ";" << endl;
    result_ << "}" << endl;
    result_ << endl;
  }

  for ( auto rw_table : rw_tables ) {
    result_ << "wasm_rt_externref_t* Z_env_Z_rw_table_" << rw_table
            << "(Z_env_module_instance_t* env_module_instance) {" << endl;
    result_ << "  return & env_module_instance->rw_table_" << rw_table << ";" << endl;
    result_ << endl;
    result_ << endl;
  }

  auto rw_mem_sizes = inspector_->GetRWSizes();
  auto rw_table_sizes = inspector_->GetRWTableSizes();
  result_ << "void init_mems(Z_env_module_instance_t* env_module_instance) {" << endl;
  for ( const auto& ro_mem : ro_mems ) {
    result_ << "  wasm_rt_allocate_memory_sw_checked(&env_module_instance->ro_mem_" << ro_mem << ", " << 0
            << ", 65536);" << endl;
  }
  for ( const auto& [rw_mem, size] : rw_mem_sizes ) {
    result_ << "  wasm_rt_allocate_memory_sw_checked(&env_module_instance->rw_mem_" << rw_mem << ", " << size
            << ", 65536);" << endl;
  }
  result_ << "  return;" << endl;
  result_ << "}" << endl;
  result_ << endl;

  result_ << "void free_mems(Z_env_module_instance_t* env_module_instance) {" << endl;
  for ( const auto& [rw_mem, size] : rw_mem_sizes ) {
    result_ << "  if (env_module_instance->rw_mem_" << rw_mem << ".data != NULL) {" << endl;
    result_ << "    wasm_rt_free_memory_sw_checked(&env_module_instance->rw_mem_" << rw_mem << ");" << endl;
    result_ << "  }" << endl;
  }
  result_ << "  return;" << endl;
  result_ << "}" << endl;
  result_ << endl;

  result_ << "void init_tables(Z_env_module_instance_t* env_module_instance) {" << endl;
  for ( const auto& ro_table : ro_tables ) {
    result_ << "  wasm_rt_allocate_externref_table(&env_module_instance->ro_table_" << ro_table << ", 0, 65536);"
            << endl;
  }
  for ( const auto& [rw_table, size] : rw_table_sizes ) {
    result_ << "  wasm_rt_allocate_externref_table(&env_module_instance->rw_table_" << rw_table << ", " << size
            << ", 65536);" << endl;
  }
  result_ << "  return;" << endl;
  result_ << "}" << endl;
  result_ << endl;

  result_ << "void free_tables(Z_env_module_instance_t* env_module_instance) {" << endl;
  for ( const auto& [rw_table, size] : rw_table_sizes ) {
    result_ << "  if (env_module_instance->rw_table_" << rw_table << ".data != NULL) {" << endl;
    result_ << "    wasm_rt_free_externref_table(&env_module_instance->rw_table_" << rw_table << ");" << endl;
    result_ << "  }" << endl;
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

  write_env_instance();
  write_attach_tree();
  write_attach_blob();
  write_detach_mem();
  write_freeze_blob();
  write_designate_output();
  write_set_ro_handles();

  result_ << "extern void* fixpoint_init_module_instance(size_t);" << endl;
  result_ << "extern void* fixpoint_init_env_module_instance(size_t);" << endl;
  result_ << "void* initProgram(__m256i encode_name) {" << endl;
  result_ << "  " << state_info_type_name_ << "* instance = (" << state_info_type_name_
          << "*)fixpoint_init_module_instance(sizeof(" << state_info_type_name_ << "));" << endl;
  result_ << "  Z_env_module_instance_t* env_module_instance = "
             "fixpoint_init_env_module_instance(sizeof(Z_env_module_instance_t));"
          << endl;
  result_ << "  init_mems(env_module_instance);" << endl;
  result_ << "  init_tables(env_module_instance);" << endl;
  result_ << "  env_module_instance->module_instance = instance;" << endl;
  result_ << "  " << module_prefix_ << "init_module();" << endl;
  result_ << "  " << module_prefix_ << "init(instance, env_module_instance);" << endl;
  result_ << "  " << module_prefix_ << "Z_ro_handles(instance)->data[0] = encode_name;" << endl;
  result_ << "  return (void*)instance;" << endl;
  result_ << "}" << endl;

  result_ << endl;
  result_ << "__m256i executeProgram(void* instance) {" << endl;
  result_ << "  return " << module_prefix_ << "Z__fixpoint_apply((" << state_info_type_name_ << "*)instance);"
          << endl;
  result_ << "}" << endl;

  result_ << endl;
  result_ << "extern void fixpoint_free_env_module_instance(void*);" << endl;
  result_ << "void cleanupProgram(void* instance) {" << endl;
  result_ << "  " << module_prefix_ << "free((" << state_info_type_name_ << "*)instance);" << endl;
  result_ << "  free_mems(((" << state_info_type_name_ << "*)instance)->Z_env_module_instance);" << endl;
  result_ << "  free_tables(((" << state_info_type_name_ << "*)instance)->Z_env_module_instance);" << endl;
  result_ << "  fixpoint_free_env_module_instance(((" << state_info_type_name_
          << "*)instance)->Z_env_module_instance);" << endl;
  result_ << "}" << endl;

  return result_.str();
}

string compose_header( string wasm_name, Module* module, Errors* error, wasminspector::WasmInspector* inspector )
{
  InitComposer composer( wasm_name, module, error, inspector );
  return composer.compose_header();
}
}
