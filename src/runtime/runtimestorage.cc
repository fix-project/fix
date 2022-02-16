#include <cassert>
#include <iostream>

#include "elfloader.hh"
#include "runtimestorage.hh"
#include "sha256.hh"
#include "timing_helper.hh"
#include "wasm-rt-content.h"

using namespace std;

Name RuntimeStorage::addBlob( string&& blob_content )
{
  if ( blob_content.length() > 32 ) {
    throw runtime_error( "Unimplemented" );
  } else {
    Name name( move( blob_content ), NameType::Literal, ContentType::Blob );
    return name;
  }
}

string_view RuntimeStorage::getBlob( const Name& name )
{
  switch ( name.getType() ) {
    case NameType::Literal:
      if ( name.getContentType() == ContentType::Blob ) {
        return name.getContent();
      }
      break;

    default:
      const Object& obj = storage.get( name );
      if ( holds_alternative<Blob>( obj ) ) {
        return get<Blob>( obj ).content();
      }
      break;
  }

  throw out_of_range( "Blob does not exist." );
}

Name RuntimeStorage::addTree( vector<TreeEntry>&& tree_content )
{
  if ( tree_content.size() > 32 ) {
    throw runtime_error( "Unimplemented." );
  } else {
    string name_content( (const char*)( tree_content.data() ), tree_content.size() * sizeof( TreeEntry ) );
    Name name( move( name_content ), NameType::Literal, ContentType::Tree );
    return name;
  }
}

span_view<TreeEntry> RuntimeStorage::getTree( const Name& name )
{
  switch ( name.getType() ) {
    case NameType::Literal:
      if ( name.getContentType() == ContentType::Tree ) {
        return span_view<TreeEntry>( name.getContent() );
      }
      break;

    default:
      const Object& obj = storage.get( name );
      if ( holds_alternative<Tree>( obj ) ) {
        return get<Tree>( obj ).content();
      }
      break;
  }

  throw out_of_range( "Tree does not exist." );
}

void RuntimeStorage::addWasm( const string& name, const string& wasm_content, const vector<string>& deps )
{
  auto [c_header, h_header] = wasmcompiler::wasm_to_c( name, wasm_content );

  string elf_content = c_to_elf( name, c_header, h_header, wasm_rt_content );

  addProgram( name, vector<string>(), vector<string>(), elf_content );
  name_to_program_.at( name ).setDeps( deps );
}

void RuntimeStorage::addProgram( const string& name,
                                 vector<string>&& inputs,
                                 vector<string>&& outputs,
                                 string& program_content )
{
  auto elf_info = load_program( program_content );
  name_to_program_.insert_or_assign( name, link_program( elf_info, name, move( inputs ), move( outputs ) ) );
}
