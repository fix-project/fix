#include <cassert>
#include <iostream>

#include "elfloader.hh"
#include "runtimestorage.hh"
#include "sha256.hh"
#include "wasm-rt-content.h"

using namespace std;

Name RuntimeStorage::add_blob( Blob&& blob )
{
  if ( blob.size() > 32 ) {
    string hash = sha256::encode( blob.content() );
    Name name( hash, ContentType::Blob );
    storage.put( name, move( blob ) );
    return name;
  } else {
    Name name( blob.content() );
    return name;
  }
}

Name RuntimeStorage::add_local_blob( Blob&& blob )
{
  if ( blob.size() > 32 ) {
    Name name( next_local_name_, ContentType::Blob );
    next_local_name_++;
    storage.put( name, move( blob ) );
    return name;
  } else {
    Name name( blob.content() );
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

Name RuntimeStorage::add_tree( Tree&& tree )
{
  string hash = sha256::encode( string_view { reinterpret_cast<const char*>( tree.content().data() ),
                                              tree.content().size() * sizeof( Name ) } );
  Name name( hash, ContentType::Tree );
  storage.put( name, move( tree ) );
  return name;
}

Name RuntimeStorage::add_local_tree( Tree&& tree )
{
  Name name( next_local_name_, ContentType::Tree );
  next_local_name_++;
  storage.put( name, move( tree ) );
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
        return force_tree( name );

      case ContentType::Thunk:
        return force_thunk( name );

      default:
        throw runtime_error( "Invalid content type." );
    }
  }
}

// XXX should force tree in-place rather than constructing new one
Name RuntimeStorage::force_tree( Name name )
{
  const auto orig_tree = get_tree( name );
  Name* new_tree = static_cast<Name*>( aligned_alloc( alignof( Name ), sizeof( Name ) * orig_tree.size() ) );
  if ( not new_tree ) {
    throw bad_alloc();
  }
  for ( size_t i = 0; i < orig_tree.size(); ++i ) {
    const auto& entry = orig_tree[i];
    if ( entry.is_strict_tree_entry() ) {
      new_tree[i] = force( entry );
    } else {
      new_tree[i] = entry;
    }
  }

  return add_tree( { new_tree, orig_tree.size() } );
}

Name RuntimeStorage::force_thunk( Name name )
{
  Name current_name = name;
  while ( true ) {
    Name new_name = reduce_thunk( current_name );
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
    Name encode_name = get_thunk_encode_name( name );
    Name result = evaluate_encode( encode_name );
    memoization_cache[name] = result;
    return result;
  }
}

Name RuntimeStorage::evaluate_encode( Name encode_name )
{
  Name forced_encode = force_tree( encode_name );
  Name forced_encode_thunk = Name::get_thunk_name( forced_encode );
  if ( memoization_cache.contains( forced_encode_thunk ) ) {
    return memoization_cache.at( forced_encode_thunk );
  }
  Name function_name = get_tree( forced_encode ).at( 1 );
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
  const auto [c_header, h_header, fixpoint_header] = wasmcompiler::wasm_to_c( name, wasm_content );

  name_to_program_.insert_or_assign(
    name, link_program( c_to_elf( name, c_header, h_header, fixpoint_header, wasm_rt_content ), name ) );
}
