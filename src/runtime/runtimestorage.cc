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

Name RuntimeStorage::getThunkEncodeName( const Name& name )
{
  switch ( name.getType() ) {
    case NameType::Literal:
      if ( name.getContentType() == ContentType::Thunk ) {
        return ( (const Thunk*)( name.getContent().data() ) )->getEncode();
      }
      break;

    default:
      const Object& obj = storage.get( name );
      if ( holds_alternative<Thunk>( obj ) ) {
        return get<Thunk>( obj ).getEncode();
      }
      break;
  }

  throw out_of_range( "Thunk does not exist." );
}

Name RuntimeStorage::force( Name name )
{
  switch ( name.getContentType() ) {
    case ContentType::Blob:
      return name;

    case ContentType::Tree:
      return this->forceTree( name );

    case ContentType::Thunk:
      return this->forceThunk( name );

    case ContentType::Unknown:
      throw runtime_error( "Unimplemented." );

    default:
      throw runtime_error( "Invalid content type." );
  }
}

Name RuntimeStorage::forceTree( Name name )
{
  vector<TreeEntry> tree_content;
  for ( const auto& entry : getTree( name ) ) {
    if ( entry.second == Laziness::Strict ) {
      tree_content.push_back( TreeEntry( this->force( entry.first ), entry.second ) );
    } else {
      tree_content.push_back( entry );
    }
  }

  return this->addTree( move( tree_content ) );
}

Name RuntimeStorage::forceThunk( Name name )
{
  while ( true ) {
    Name new_name = this->reduceThunk( name );
    switch ( new_name.getContentType() ) {
      case ContentType::Blob:
      case ContentType::Tree:
        return new_name;

      default:
        name = new_name;
    }
  }
}

Name RuntimeStorage::reduceThunk( Name name )
{
  if ( memorization_cache.contains( name ) ) {
    return memorization_cache.at( name );
  } else {
    Name encode_name = this->getThunkEncodeName( name );
    return this->evaluateEncode( encode_name );
  }
}

Name RuntimeStorage::evaluateEncode( Name encode_name )
{
  if ( this->getTree( encode_name ).size() == 1 ) {
    throw runtime_error( "Invalid encode!" );
  }

  Name forced_encode = this->forceTree( encode_name );
  Name res_name( forced_encode.getContent(), NameType::Canonical, ContentType::Thunk );
  if ( memorization_cache.contains( res_name ) ) {
    return memorization_cache.at( res_name );
  }

  Name function_name = this->getTree( encode_name ).at( 1 ).first;
  string program_name = function_name.getContent();
  name_to_program_.at( program_name ).execute();

  // TODO
  return encode_name;
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
