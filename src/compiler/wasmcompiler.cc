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

//  static int s_verbose;
static std::string s_infile;
static std::string s_outfile;
static Features s_features;
static WriteCOptions s_write_c_options;
static bool s_read_debug_names = true;
static std::unique_ptr<FileStream> s_log_stream;

MemoryStringStream::MemoryStringStream( Stream* log_stream )
  : Stream( log_stream )
{}

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

tuple<string, string, string> wasm_to_c( const string& wasm_name, const string& wasm_content )
{
  Result result;

  Errors errors;
  Module module;
  const bool kStopOnFirstError = true;
  const bool kFailOnCustomSectionError = true;

  s_features.enable_multi_memory();

  ReadBinaryOptions options(
    s_features, s_log_stream.get(), s_read_debug_names, kStopOnFirstError, kFailOnCustomSectionError );
  result = ReadBinaryIr( wasm_name.c_str(), wasm_content.data(), wasm_content.size(), options, &errors, &module );
  MemoryStringStream c_stream;
  MemoryStringStream h_stream;
  string fixpoint_header;
  if ( Succeeded( result ) ) {
    if ( Succeeded( result ) ) {
      ValidateOptions v_options( s_features );
      result = ValidateModule( &module, &errors, v_options );
      result |= GenerateNames( &module );
    }

    if ( Succeeded( result ) ) {
      /* TODO(binji): This shouldn't fail; if a name can't be applied
       * (because the index is invalid, say) it should just be skipped. */
      Result dummy_result = ApplyNames( &module );
      WABT_USE( dummy_result );
    }

    wasminspector::WasmInspector inspector( &module, &errors );

    if ( Succeeded( result ) ) {
      result = inspector.Validate();
    }
    if ( result != Result::Ok ) {
      throw runtime_error( "Invalid module." );
    }

    if ( Succeeded( result ) ) {
      for ( auto index : inspector.GetExportedROMemIndex() ) {
        module.memories[index]->bounds_checked = true;
      }
      for ( auto index : inspector.GetExportedRWMemIndex() ) {
        module.memories[index]->bounds_checked = true;
      }
    }

    if ( Succeeded( result ) ) {
      result = WriteC( &c_stream, &h_stream, ( wasm_name + ".h" ).c_str(), &module, s_write_c_options );
    }

    if ( Succeeded( result ) ) {
      fixpoint_header = initcomposer::compose_header( wasm_name, &module, &errors, &inspector );
    }
  }
  FormatErrorsToFile( errors, Location::Type::Binary );
  if ( result != Result::Ok ) {
    throw runtime_error( "WasmToC fails." );
  }

  return { c_stream.ReleaseStringBuf(), h_stream.ReleaseStringBuf(), fixpoint_header };
}
}
