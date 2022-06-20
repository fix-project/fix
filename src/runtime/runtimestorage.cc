#include <cassert>
#include <iostream>

#include "elfloader.hh"
#include "object.hh"
#include "runtimestorage.hh"
#include "sha256.hh"
#include "wasm-rt-content.h"

using namespace std;

Name RuntimeStorage::add_blob( Blob&& blob )
{
  if ( blob.size() > 32 ) {
    Name name( local_storage_.size(), ContentType::Blob );
    local_storage_.push_back( move( blob ) );
    return name;
  } else {
    Name name( blob );
    return name;
  }
}

string_view RuntimeStorage::get_blob( Name name )
{
  if ( name.is_literal_blob() ) {
    return name.literal_blob();
  } else if ( name.is_local() ) {
    const ObjectOrName& obj = local_storage_.at( name.get_local_id() );
    if ( holds_alternative<Blob>( obj ) ) {
      return get<Blob>( obj );
    } else if ( holds_alternative<Name>( obj ) ) {
      return get_blob( get<Name>( obj ) );
    }
  } else {
    const Object& obj = storage.get( name );
    if ( holds_alternative<Blob>( obj ) ) {
      return get<Blob>( obj );
    }
  }

  throw out_of_range( "Blob does not exist." );
}

string_view RuntimeStorage::user_get_blob( const Name& name )
{
  if ( name.is_literal_blob() ) {
    return name.literal_blob();
  } else if ( name.is_local() ) {
    const ObjectOrName& obj = local_storage_.at( name.get_local_id() );
    if ( holds_alternative<Blob>( obj ) ) {
      return get<Blob>( obj );
    } else if ( holds_alternative<Name>( obj ) ) {
      return get_blob( get<Name>( obj ) );
    }
  } else {
    const Object& obj = storage.get( name );
    if ( holds_alternative<Blob>( obj ) ) {
      return get<Blob>( obj );
    }
  }

  throw out_of_range( "Blob does not exist." );
}

Name RuntimeStorage::add_tree( Tree&& tree )
{
  Name name( local_storage_.size(), ContentType::Tree );
  local_storage_.push_back( move( tree ) );
  return name;
}

span_view<Name> RuntimeStorage::get_tree( Name name )
{
  if ( name.is_local() ) {
    const ObjectOrName& obj = local_storage_.at( name.get_local_id() );
    if ( holds_alternative<Tree>( obj ) ) {
      return get<Tree>( obj );
    } else if ( holds_alternative<Name>( obj ) ) {
      return get_tree( get<Name>( obj ) );
    }
  } else {
    const Object& obj = storage.get( name );
    if ( holds_alternative<Tree>( obj ) ) {
      return get<Tree>( obj );
    }
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

Name RuntimeStorage::force_tree( Name name )
{
  auto orig_tree = get_tree( name );

  for ( size_t i = 0; i < orig_tree.size(); ++i ) {
    auto entry = orig_tree[i];
    if ( entry.is_strict_tree_entry() && !entry.is_blob() ) {
      orig_tree.mutable_data()[i] = force( entry );
    }
  }

  return name;
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
  // if ( memoization_cache.contains( name ) ) {
  //  return memoization_cache.at( name );
  //} else {
  Name encode_name = get_thunk_encode_name( name );
  Name result = evaluate_encode( encode_name );
  //  memoization_cache[name] = result;
  return result;
  //}
}

Name RuntimeStorage::evaluate_encode( Name encode_name )
{
  force_tree( encode_name );
  Name function_name = get_tree( encode_name ).at( 1 );

  if ( not function_name.is_blob() ) {
    throw runtime_error( "ENCODE functions not yet supported" );
  }

  if ( not name_to_program_.contains( function_name ) ) {
    /* compile the Wasm to C and then to ELF */
    const auto [c_header, h_header, fixpoint_header] = wasmcompiler::wasm_to_c( get_blob( function_name ) );

    name_to_program_.insert_or_assign(
      function_name, link_program( c_to_elf( c_header, h_header, fixpoint_header, wasm_rt_content ) ) );
  }

  auto& program = name_to_program_.at( function_name );
  __m256i output = program.execute( encode_name );

  return output;
}

void RuntimeStorage::populate_program( Name function_name )
{
  if ( not function_name.is_blob() ) {
    throw runtime_error( "ENCODE functions not yet supported" );
  }

  if ( not name_to_program_.contains( function_name ) ) {
    /* compile the Wasm to C and then to ELF */
    const auto [c_header, h_header, fixpoint_header] = wasmcompiler::wasm_to_c( get_blob( function_name ) );

    name_to_program_.insert_or_assign(
      function_name, link_program( c_to_elf( c_header, h_header, fixpoint_header, wasm_rt_content ) ) );
  }

  return;
}
