#include <cassert>
#include <iostream>

#include "elfloader.hh"
#include "runtimestorage.hh"
#include "sha256.hh"
#include "timing_helper.hh"
#include "wasm-rt-content.h"

using namespace std;

Name RuntimeStorage::add_blob( string&& blob_content )
{
  if ( blob_content.length() > 32 ) {
    throw runtime_error( "Unimplemented" );
  } else {
    Name name( blob_content );
    return name;
  }
}

string_view RuntimeStorage::get_blob( const Name& name )
{
  if ( name.is_literal_blob() ) {
    return name.literal_blob();
  } else {
    const Object& obj = storage.get( name );
    if ( holds_alternative<Blob>( obj ) ) {
      return get<Blob>( obj ).content();
    }
  }

  throw out_of_range( "Blob does not exist." );
}

Name RuntimeStorage::add_tree( vector<Name>&& tree_content )
{
  string hash = sha256::encode(
    string_view { reinterpret_cast<char*>( tree_content.data() ), tree_content.size() * sizeof( Name ) } );
  Name name( hash, ContentType::Tree );
  storage.put( name, Tree( move( tree_content ) ) );
  return name;
}

span_view<Name> RuntimeStorage::get_tree( const Name& name )
{
  const Object& obj = storage.get( name );
  if ( holds_alternative<Tree>( obj ) ) {
    return get<Tree>( obj ).content();
  }
  throw out_of_range( "Tree does not exist." );
}

Name RuntimeStorage::add_thunk( Thunk thunk )
{
  return thunk.get_encode().get_thunk_name();
}

Name RuntimeStorage::get_thunk_encode_name( const Name& thunk_name )
{
  return thunk_name.get_encode_name();
}

Name RuntimeStorage::force( const Name& name )
{
  if ( name.is_literal_blob() ) {
    return name;
  } else {
    switch ( name.get_content_type() ) {
      case ContentType::Blob:
        return name;

      case ContentType::Tree:
        return this->force_tree( name );

      case ContentType::Thunk:
        return this->force_thunk( name );

      default:
        throw runtime_error( "Invalid content type." );
    }
  }
}

Name RuntimeStorage::force_tree( const Name& name )
{
  vector<Name> tree_content;
  for ( const auto& entry : get_tree( name ) ) {
    if ( entry.is_strict_tree_entry() ) {
      tree_content.push_back( this->force( entry ) );
    } else {
      tree_content.push_back( entry );
    }
  }

  return this->add_tree( move( tree_content ) );
}

Name RuntimeStorage::force_thunk( const Name& name )
{
  Name current_name = name;
  while ( true ) {
    Name new_name = this->reduce_thunk( current_name );
    switch ( new_name.get_content_type() ) {
      case ContentType::Blob:
      case ContentType::Tree:
        return new_name;

      default:
        current_name = new_name;
    }
  }
}

Name RuntimeStorage::reduce_thunk( const Name& name )
{
  if ( memorization_cache.contains( name ) ) {
    return memorization_cache.at( name );
  } else {
    Name encode_name = this->get_thunk_encode_name( name );
    return this->evaluate_encode( encode_name );
  }
}

Name RuntimeStorage::evaluate_encode( const Name& encode_name )
{
  Name forced_encode = this->force_tree( encode_name );
  Name forced_encode_thunk = forced_encode.get_thunk_name();
  if ( memorization_cache.contains( forced_encode_thunk ) ) {
    return memorization_cache.at( forced_encode_thunk );
  }

  Name function_name = this->get_tree( forced_encode ).at( 1 );
  string program_name = string( function_name.literal_blob() );
  void* wasm_instance = name_to_program_.at( program_name ).execute( forced_encode );

  Instance* fixpoint_instance = reinterpret_cast<Instance*>( wasm_instance ) - 1;
  Name output = fixpoint_instance->get_output();

  name_to_program_.at( program_name ).cleanup( wasm_instance );
  free( (void*)fixpoint_instance );

  return output;
}

void RuntimeStorage::add_wasm( const string& name, const string& wasm_content )
{
  auto [c_header, h_header, fixpoint_header] = wasmcompiler::wasm_to_c( name, wasm_content );

  string elf_content = c_to_elf( name, c_header, h_header, fixpoint_header, wasm_rt_content );

  add_program( name, elf_content );
}

void RuntimeStorage::add_program( const string& name, string& program_content )
{
  auto elf_info = load_program( program_content );
  name_to_program_.insert_or_assign( name, link_program( elf_info, name ) );
}
