#include "wasmcompiler.hh"

#include "src/apply-names.h"
#include "src/binary-reader-ir.h"
#include "src/binary-reader.h"
#include "src/error-formatter.h"
#include "src/feature.h"
#include "src/generate-names.h"
#include "src/ir.h"
#include "src/option-parser.h"
#include "src/validator.h"
#include "src/wast-lexer.h"

#include "src/c-writer.h"

#include "initcomposer.hh"
#include "wasminspector.hh"

using namespace std;
using namespace wabt;

namespace wasmcompiler {

MemoryStringStream::MemoryStringStream( Stream* log_stream )
  : Stream( log_stream )
{
}

string&& MemoryStringStream::ReleaseStringBuf()
{
  return move( buf_ );
}

Result MemoryStringStream::WriteDataImpl( size_t dst_offset, const void* src, size_t size )
{
  if ( size == 0 ) {
    return Result::Ok;
  }
  size_t end = dst_offset + size;
  if ( end > buf_.size() ) {
    buf_.resize( end );
  }
  char* dst = &buf_[dst_offset];
  memcpy( dst, src, size );
  return Result::Ok;
}

Result MemoryStringStream::MoveDataImpl( size_t dst_offset, size_t src_offset, size_t size )
{
  if ( size == 0 ) {
    return Result::Ok;
  }
  size_t src_end = src_offset + size;
  size_t dst_end = dst_offset + size;
  size_t end = src_end > dst_end ? src_end : dst_end;
  if ( end > buf_.size() ) {
    buf_.resize( end );
  }

  char* dst = &buf_[dst_offset];
  char* src = &buf_[src_offset];
  memmove( dst, src, size );
  return Result::Ok;
}

Result MemoryStringStream::TruncateImpl( size_t size )
{
  if ( size > buf_.size() ) {
    return Result::Error;
  }
  buf_.resize( size );
  return Result::Ok;
}

void wabt_try( const string_view what, const Errors& errors, const Result value )
{
  if ( not Succeeded( value ) ) {
    const string error_str = FormatErrorsToString( errors, Location::Type::Binary );
    throw runtime_error( "wabt error: " + string( what ) + " failed\n\n===\n\n" + error_str + "\n\n===\n" );
  }
}

tuple<string, string, string> wasm_to_c( const string& wasm_name, const string& wasm_content )
{
  Errors errors;
  Module module;

  ReadBinaryOptions options;
  options.features.enable_multi_memory();

  wabt_try(
    "ReadBinaryIr",
    errors,
    ReadBinaryIr( wasm_name.c_str(), wasm_content.data(), wasm_content.size(), options, &errors, &module ) );

  MemoryStringStream c_stream;
  MemoryStringStream h_stream;
  string fixpoint_header;

  wabt_try( "ValidateModule", errors, ValidateModule( &module, &errors, options.features ) );
  wabt_try( "GenerateNames", errors, GenerateNames( &module ) );
  wabt_try( "ApplyNames", errors, ApplyNames( &module ) );

  wasminspector::WasmInspector inspector( &module, &errors );
  wabt_try( "Inspector validate", errors, inspector.Validate() );

  for ( auto index : inspector.GetExportedROMemIndex() ) {
    module.memories[index]->bounds_checked = true;
  }
  for ( auto index : inspector.GetExportedRWMemIndex() ) {
    module.memories[index]->bounds_checked = true;
  }

  WriteCOptions write_c_options;
  write_c_options.module_name = wasm_name;
  string header_name = wasm_name + ".h";
  wabt_try( "WriteC", errors, WriteC( &c_stream, &h_stream, header_name.c_str(), &module, write_c_options ) );

  fixpoint_header = initcomposer::compose_header( wasm_name, &module, &errors, &inspector );

  return { c_stream.ReleaseStringBuf(), h_stream.ReleaseStringBuf(), fixpoint_header };
}
} /* namespace wasmcompiler */
