#include <cassert>
#include <iostream>

#include "elfloader.hh"
#include "runtimestorage.hh"
#include "sha256.hh"
#include "timing_helper.hh"
#include "wasm-rt-content.h"

using namespace std;

string_view RuntimeStorage::getBlob( const Name& name )
{
  if ( name_to_thunk_.contains( name ) ) {
    Name temp_name = name;

    while ( name_to_thunk_.contains( temp_name ) ) {
      const auto& thunk = name_to_thunk_.get( temp_name );
      temp_name = Name( thunk.getEncode(), thunk.getPath(), ContentType::Unknown );
    }

    return name_to_blob_.get( temp_name ).content();
  }

  switch ( name.getType() ) {
    case NameType::Literal:
      return name.getContent();

    case NameType::Canonical:
      return name_to_blob_.get( name ).content();

    case NameType::Thunk:
      return name_to_blob_.get( name ).content();

    default:
      throw out_of_range( "Blob does not exist." );
  }
}

const Tree& RuntimeStorage::getTree( const Name& name )
{
  switch ( name.getType() ) {
    case NameType::Literal:
      return name.getTreeContent();

    case NameType::Canonical:
    case NameType::Thunk:
      return name_to_tree_.get( name );

    default:
      throw out_of_range( "Tree does not exist." );
  }
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

Name RuntimeStorage::force( Name name )
{
  switch ( name.getContentType() ) {
    case ContentType::Blob:
      return name;

    case ContentType::Tree:
      return this->forceTree( name );

    case ContentType::Thunk:
      switch ( name.getType() ) {
        case NameType::Literal:
          return this->forceThunk( reinterpret_cast<const Thunk&>( name.getContent() ) );

        case NameType::Canonical:
          return this->forceThunk( name_to_thunk_.get( name ) );

        case NameType::Thunk:
          return this->forceThunk( name_to_thunk_.get( name ) );

        default:
          throw runtime_error( "Invalid name type." );
      }

    case ContentType::Unknown:
      if ( name_to_tree_.contains( name ) ) {
        return this->forceTree( name_to_tree_.get( name ) );
      } else if ( thunk_to_blob_.contains( name ) ) {
        return thunk_to_blob_.at( name );
      } else if ( name_to_thunk_.contains( name ) ) {
        return this->forceThunk( name_to_thunk_.get( name ) );
      }
      throw runtime_error( "Invalid unknown content." );

    default:
      throw runtime_error( "Invalid content type." );
  }
}

Name RuntimeStorage::forceTree( Name tree_name )
{
  vector<Name> tree_content;
  for ( const auto& name : this->getTree( tree_name ) ) {
    tree_content.push_back( this->force( name ) );
  }
  return this->addTree( move( tree_content ) );
}

Name RuntimeStorage::forceThunk( const Thunk& thunk )
{
  vector<size_t> path = { 0 };
  Name forced_encode = this->forceTree( thunk.getEncode() );
  Name res_name( forced_encode, path, ContentType::Thunk );

  if ( thunk_to_blob_.contains( res_name ) ) {
    return thunk_to_blob_.at( res_name );
  }

  this->evaluateEncode( forced_encode );

  if ( name_to_thunk_.contains( res_name ) ) {
    const Thunk& sub_thunk = name_to_thunk_.get( res_name );
    Name res = this->forceThunk( sub_thunk );
    thunk_to_blob_.insert_or_assign( res_name, res );
    return res;
  } else {
    return thunk_to_blob_.at( res_name );
  }
}

void RuntimeStorage::evaluateEncode( Name encode_name )
{
  if ( this->getTree( encode_name ).size() > 3 ) {
    throw runtime_error( "Invalid encode!" );
  }

  string program_name;
  // cout << "Evaluating " << encode_name << endl;
  {
    RecordScopeTimer<Timer::Category::Nonblock> record_timer { _invocation_init };
    Name function_name = this->getTree( encode_name ).at( 0 );
    wasi::invoc_ptr.setProgramName( function_name.getContent() );
    program_name = function_name.getContent();
    wasi::invoc_ptr.resetInvocation( move( encode_name ) );
    wasi::buf.size = 0;
  }

  name_to_program_.at( program_name ).execute();

  wasi::invoc_ptr.add_to_storage();
}

Name RuntimeStorage::addBlob( string&& blob_content )
{
  if ( blob_content.length() > 32 ) {
    throw runtime_error( "Not implemented yet." );
  }

  Name name( move( blob_content ), NameType::Literal, ContentType::Blob );

  return name;
}

Name RuntimeStorage::addTree( vector<Name>&& tree_content )
{
  if ( tree_content.size() > 32 ) {
    throw runtime_error( "Not implemetned yet." );
  }

  Name name( tree_content );

  return name;
}

Name RuntimeStorage::addEncode( const Name& program_name, const Name& strict_input, const Name& lazy_input )
{
  Tree encode;

  encode.push_back( program_name );
  encode.push_back( strict_input );
  encode.push_back( lazy_input );

  return this->addTree( move( encode ) );
}

Name RuntimeStorage::addEncode( const Name& program_name, const Name& strict_input )
{
  Tree encode;

  encode.push_back( program_name );
  encode.push_back( strict_input );

  return this->addTree( move( encode ) );
}
