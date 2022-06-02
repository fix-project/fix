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
    string hash = sha256::encode( string_view { blob_content } );
    Name name( hash, ContentType::Blob );
    storage.put( name, Blob( move( blob_content ) ) );
    return name;
  } else {
    Name name( blob_content );
    return name;
  }
}

Name RuntimeStorage::add_local_blob( string&& blob_content )
{
  if ( blob_content.length() > 32 ) {
    Name name( next_local_name_, ContentType::Blob );
    next_local_name_++;
    storage.put( name, Blob( move( blob_content ) ) );
    return name;
  } else {
    Name name( blob_content );
    return name;
  }
}

string_view RuntimeStorage::get_blob( Name name )
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

string_view RuntimeStorage::user_get_blob( const Name& name )
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

Name RuntimeStorage::add_local_tree( vector<Name>&& tree_content )
{
  Name name( next_local_name_, ContentType::Tree );
  next_local_name_++;
  storage.put( name, Tree( move( tree_content ) ) );
  return name;
}
span_view<Name> RuntimeStorage::get_tree( Name name )
{
  const Object& obj = storage.get( name );
  if ( holds_alternative<Tree>( obj ) ) {
    return get<Tree>( obj ).content();
  }
  throw out_of_range( "Tree does not exist." );
}

Name RuntimeStorage::add_thunk( Thunk thunk )
{
  return Name::get_thunk_name( thunk.get_encode() );
}

Name RuntimeStorage::get_thunk_encode_name( Name thunk_name )
{
  return Name::get_encode_name( thunk_name );
}

Name RuntimeStorage::force( Name name )
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

Name RuntimeStorage::force_tree( Name name )
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

Name RuntimeStorage::force_thunk( Name name )
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

Name RuntimeStorage::reduce_thunk( Name name )
{
  if ( memoization_cache.contains( name ) ) {
    return memoization_cache.at( name );
  } else {
    Name encode_name = this->get_thunk_encode_name( name );
    Name result = this->evaluate_encode( encode_name );
    memoization_cache[name] = result;
    return result;
  }
}

Name RuntimeStorage::evaluate_encode( Name encode_name )
{
  Name forced_encode = this->force_tree( encode_name );
  Name forced_encode_thunk = Name::get_thunk_name( forced_encode );
  if ( memoization_cache.contains( forced_encode_thunk ) ) {
    return memoization_cache.at( forced_encode_thunk );
  }
  Name function_name = this->get_tree( forced_encode ).at( 1 );
  string program_name = string( function_name.literal_blob() );
  size_t instance_size = name_to_program_.at( program_name ).get_instance_and_context_size();
  void* ptr = aligned_alloc( alignof( __m256i ), instance_size );
  memset( ptr, 0, instance_size );
  string_span instance { static_cast<const char*>( ptr ), instance_size };

  name_to_program_.at( program_name ).populate_instance_and_context( instance );
  __m256i output = name_to_program_.at( program_name ).execute( forced_encode, instance );
  name_to_program_.at( program_name ).cleanup( instance );
  free( ptr );

  memoization_cache[forced_encode_thunk] = Name( output );
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
  name_to_program_.insert_or_assign( name, link_program( program_content, name ) );
}
