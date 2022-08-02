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
    , module_prefix_( MangleName( wasm_name ) )
    , result_()
    , inspector_( inspector )
  {
  }

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
  void write_memory_size();
  void write_create_blob();
  void write_create_blob_i32();
  void write_value_type();
  void write_create_tree();
  void write_create_thunk();
  void write_init_read_only_mem_table();
  void write_get_instance_size();
  void write_context();
};

void InitComposer::write_context()
{
  result_ << "typedef struct Context {" << endl;
  result_ << "  __m256i return_value;" << endl;
  result_ << "  size_t memory_usage;" << endl;
  result_ << "} Context;\n" << endl;

  result_ << "Context* get_context_ptr( void* instance ) {" << endl;
  result_ << "  return (Context*)((char*)instance + get_instance_size());" << endl;
  result_ << "}\n" << endl;
}

void InitComposer::write_attach_tree()
{
  result_ << "extern void fixpoint_attach_tree(__m256i, wasm_rt_externref_table_t*);" << endl;
  auto ro_tables = inspector_->GetExportedROTables();
  for ( uint32_t idx : ro_tables ) {
    result_ << "void Z_fixpointZ_attach_tree_ro_table_" << idx
            << "(struct Z_fixpoint_module_instance_t* module_instance, __m256i ro_handle) {" << endl;
    result_ << "  wasm_rt_externref_table_t* ro_table = " << module_prefix_ << "Z_ro_table_" << idx << "(("
            << state_info_type_name_ << "*)module_instance);" << endl;
    result_ << "  fixpoint_attach_tree(ro_handle, ro_table);" << endl;
    result_ << "}\n" << endl;
  }
}

void InitComposer::write_attach_blob()
{
  auto ro_mems = inspector_->GetExportedROMems();
  result_ << "extern void fixpoint_attach_blob(__m256i, wasm_rt_memory_t*);" << endl;
  for ( uint32_t idx : ro_mems ) {
    result_ << "void Z_fixpointZ_attach_blob_ro_mem_" << idx
            << "(struct Z_fixpoint_module_instance_t* module_instance, __m256i ro_handle) {" << endl;
    result_ << "  wasm_rt_memory_t* ro_mem = " << module_prefix_ << "Z_ro_mem_" << idx << "(("
            << state_info_type_name_ << "*)module_instance);" << endl;
    result_ << "  fixpoint_attach_blob(ro_handle, ro_mem);" << endl;
    result_ << "}\n" << endl;
  }
}

void InitComposer::write_memory_size()
{
  auto ro_mems = inspector_->GetExportedROMems();
  for ( uint32_t idx : ro_mems ) {
    result_ << "uint32_t Z_fixpointZ_size_ro_mem_" << idx
            << "(struct Z_fixpoint_module_instance_t* module_instance) {" << endl;
    result_ << "  wasm_rt_memory_t* ro_mem = " << module_prefix_ << "Z_ro_mem_" << idx << "(("
            << state_info_type_name_ << "*)module_instance);" << endl;
    result_ << "  return ro_mem->size;" << endl;
    result_ << "}\n" << endl;
  }
}

void InitComposer::write_create_blob()
{
  auto rw_mems = inspector_->GetExportedRWMems();
  result_ << "extern __m256i fixpoint_create_blob( wasm_rt_memory_t*, uint32_t );" << endl;
  for ( uint32_t idx : rw_mems ) {
    result_ << "__m256i Z_fixpointZ_create_blob_rw_mem_" << idx
            << "(struct Z_fixpoint_module_instance_t* module_instance, uint32_t size) {" << endl;
    result_ << "  wasm_rt_memory_t* rw_mem = " << module_prefix_ << "Z_rw_mem_" << idx << "(("
            << state_info_type_name_ << "*)module_instance);" << endl;
    result_ << "  return fixpoint_create_blob(rw_mem, size);" << endl;
    result_ << "}\n" << endl;
  }
}

void InitComposer::write_create_blob_i32()
{
  result_ << "extern __m256i fixpoint_create_blob_i32( uint32_t );" << endl;
  result_ << "__m256i Z_fixpointZ_create_blob_i32"
          << "(struct Z_fixpoint_module_instance_t* module_instance, uint32_t content) {" << endl;
  result_ << "  return fixpoint_create_blob_i32( content );" << endl;
  result_ << "}\n" << endl;
}

void InitComposer::write_value_type()
{
  result_ << "extern uint32_t fixpoint_value_type( __m256i );" << endl;
  result_ << "uint32_t Z_fixpointZ_value_type"
          << "(struct Z_fixpoint_module_instance_t* module_instance, __m256i handle ) {" << endl;
  result_ << "  return fixpoint_value_type( handle );" << endl;
  result_ << "}\n" << endl;
}

void InitComposer::write_create_tree()
{
  auto rw_tables = inspector_->GetExportedRWTables();
  result_ << "extern __m256i fixpoint_create_tree( wasm_rt_externref_table_t*, uint32_t );" << endl;
  for ( auto rw_table : rw_tables ) {
    result_ << "__m256i Z_fixpointZ_create_tree_rw_table_" << rw_table
            << "(struct Z_fixpoint_module_instance_t* module_instance, uint32_t size) {" << endl;
    result_ << "  wasm_rt_externref_table_t* rw_table = " << module_prefix_ << "Z_rw_table_" << rw_table << "(("
            << state_info_type_name_ << "*)module_instance);" << endl;
    result_ << "  return fixpoint_create_tree(rw_table, size);" << endl;
    result_ << "}\n" << endl;
  }
}

void InitComposer::write_create_thunk()
{
  result_ << "extern __m256i fixpoint_create_thunk(__m256i);" << endl;
  result_ << "__m256i " << module_prefix_
          << "Z_fixpointZ_create_thunk(struct Z_fixpoint_module_instance_t* module_instance, __m256i handle) {"
          << endl;
  result_ << "  return fixpoint_create_thunk(handle);" << endl;
  result_ << "}\n" << endl;
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

void InitComposer::write_get_instance_size()
{
  result_ << "size_t get_instance_size() {" << endl;
  result_ << "  return sizeof(" << state_info_type_name_ << ");" << endl;
  result_ << "}\n" << endl;
}

string InitComposer::compose_header()
{
  result_ = ostringstream();
  result_ << "#include <immintrin.h>" << endl;
  result_ << "#include \"" << wasm_name_ << "_fixpoint.h\"" << endl;
  result_ << endl;

  write_get_instance_size();
  write_context();
  write_init_read_only_mem_table();
  write_attach_tree();
  write_attach_blob();
  write_memory_size();
  write_create_tree();
  write_create_blob();
  write_create_blob_i32();
  write_value_type();
  write_create_thunk();

  result_ << "void initProgram(void* ptr) {" << endl;
  result_ << "  " << state_info_type_name_ << "* instance = (" << state_info_type_name_ << "*)ptr;" << endl;
  result_ << "  " << module_prefix_ << "_instantiate(instance, (struct Z_fixpoint_module_instance_t*)instance);"
          << endl;
  result_ << "  init_mems(instance);" << endl;
  result_ << "  init_tables(instance);" << endl;
  result_ << "  return;" << endl;
  result_ << "}" << endl;

  return result_.str();
}

string compose_header( string wasm_name, Module* module, Errors* error, wasminspector::WasmInspector* inspector )
{
  InitComposer composer( wasm_name, module, error, inspector );
  return composer.compose_header();
}
}
