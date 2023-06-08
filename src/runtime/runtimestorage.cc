#include "base64.hh"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "job.hh"
#include "object.hh"
#include "runtimestorage.hh"
#include "sha256.hh"
#include "wasm-rt-content.h"

using namespace std;

bool RuntimeStorage::steal_work( Job& job, size_t tid )
{
  // Starting at the next thread index after the current thread--look to steal work
  for ( size_t i = 0; i < num_workers_; ++i ) {
    if ( workers_[( i + tid + 1 ) % num_workers_]->jobs_.pop( job ) ) {
      return true;
    }
  }
  return false;
}

Handle RuntimeStorage::force_thunk( Handle name )
{
  Handle desired( name, false, { FORCE } );
  Handle operations( name, true, { FORCE } );

  if ( fix_cache_.try_wait( desired ) ) {
    workers_[0].get()->queue_job( Job( name, operations ) );

    std::shared_ptr<std::atomic<int64_t>> pending = fix_cache_.get_pending( desired );

    pending->wait( -2 );
    pending->wait( 1 );
    pending->wait( 0 );
  }

  return fix_cache_.get_name( desired );
}

Handle RuntimeStorage::eval_thunk( Handle name )
{
  Handle desired( name, false, { EVAL } );
  Handle operations( name, true, { EVAL } );

  if ( fix_cache_.try_wait( desired ) ) {
    workers_[0].get()->queue_job( Job( name, operations ) );

    std::shared_ptr<std::atomic<int64_t>> pending = fix_cache_.get_pending( desired );

    pending->wait( -2 );
    pending->wait( 1 );
    pending->wait( 0 );
  }

  return fix_cache_.get_name( desired );
}

Handle RuntimeStorage::add_blob( Blob&& blob )
{
  if ( blob.size() > 31 ) {
    size_t local_id = local_storage_.push_back( std::move( blob ) );
    Handle name( local_id, blob.size(), ContentType::Blob );
    return name;
  } else {
    Handle name( blob );
    std::cout << name.literal_blob() << std::endl;
    std::cout << name << std::endl;
    return name;
  }
}

string_view RuntimeStorage::get_blob( Handle name )
{
  if ( name.is_literal_blob() ) {
    return name.literal_blob();
  } else if ( name.is_local() ) {
    return get<Blob>( local_storage_.at( name.get_local_id() ) );
  } else {
    Handle local_name = fix_cache_.get_name( name );
    return get<Blob>( local_storage_.at( local_name.get_local_id() ) );
  }

  throw out_of_range( "Blob does not exist." );
}

string_view RuntimeStorage::user_get_blob( const Handle& name )
{
  if ( name.is_literal_blob() ) {
    return name.literal_blob();
  } else if ( name.is_local() ) {
    return get<Blob>( local_storage_.at( name.get_local_id() ) );
  } else {
    Handle local_name = fix_cache_.get_name( name );
    return get<Blob>( local_storage_.at( local_name.get_local_id() ) );
  }

  throw out_of_range( "Blob does not exist." );
}

Handle RuntimeStorage::add_tree( Tree&& tree )
{
  size_t local_id = local_storage_.push_back( std::move( tree ) );
  Handle name( local_id, tree.size(), ContentType::Tree );
  return name;
}

Handle RuntimeStorage::add_tag( Tree&& tree )
{
  assert( tree.size() == 3 );
  size_t local_id = local_storage_.push_back( std::move( tree ) );
  Handle name( local_id, tree.size(), ContentType::Tag );
  return name;
}

span_view<Handle> RuntimeStorage::get_tree( Handle name )
{
  if ( name.is_local() ) {
    return get<Tree>( local_storage_.at( name.get_local_id() ) );
  } else {
    Handle local_name = fix_cache_.get_name( name );
    return get<Tree>( local_storage_.at( local_name.get_local_id() ) );
  }

  throw out_of_range( "Tree does not exist." );
}

Handle RuntimeStorage::add_thunk( Thunk thunk )
{
  return Handle::get_thunk_name( thunk.get_encode() );
}

Handle RuntimeStorage::get_thunk_encode_name( Handle thunk_name )
{
  return Handle::get_encode_name( thunk_name );
}

void RuntimeStorage::populate_program( Handle function_name )
{
  if ( not function_name.is_blob() ) {
    throw runtime_error( "ENCODE functions not yet supported" );
  }

  if ( not name_to_program_.contains( function_name ) ) {
    /* Link the program */
    name_to_program_.put( function_name, link_program( get_blob( function_name ) ) );
  }

  return;
}

void RuntimeStorage::add_program( Handle function_name, string_view elf_content )
{
  if ( not name_to_program_.contains( function_name ) ) {
    name_to_program_.put( function_name, link_program( elf_content ) );
  }

  return;
}

Handle RuntimeStorage::local_to_storage( Handle name )
{
  if ( name.is_literal_blob() || !name.is_local() ) {
    return name;
  }

  switch ( name.get_content_type() ) {
    case ContentType::Blob: {
      const Object& obj = local_storage_.at( name.get_local_id() );
      if ( holds_alternative<Blob>( obj ) ) {
        string_view blob = get<Blob>( obj );

        Handle hash( sha256::encode( blob ), blob.size(), ContentType::Blob );
        fix_cache_.insert_or_assign( hash, name );

        return hash;
      } else {
        throw runtime_error( "Handle type does not match content type" );
      }

      break;
    }

    case ContentType::Tree: {
      const Object& obj = local_storage_.at( name.get_local_id() );
      if ( holds_alternative<Tree>( obj ) ) {
        span_view<Handle> orig_tree = get<Tree>( obj );

        Handle new_name = add_tree( Tree( orig_tree.size() ) );
        span_view<Handle> tree = get_tree( new_name );

        for ( size_t i = 0; i < tree.size(); ++i ) {
          auto entry = orig_tree[i];
          tree.mutable_data()[i] = local_to_storage( entry );
        }

        string_view view( reinterpret_cast<char*>( tree.mutable_data() ), tree.size() * sizeof( Handle ) );
        Handle hash( sha256::encode( view ), tree.size(), ContentType::Tree );

        fix_cache_.insert_or_assign( hash, new_name );

        return hash;
      } else {
        throw runtime_error( "Handle type does not match content type" );
      }
      break;
    }

    case ContentType::Thunk: {
      return Handle::get_thunk_name( local_to_storage( Handle::get_encode_name( name ) ) );
    }

    default:
      throw runtime_error( "Unknown content type." );
  }
}

string RuntimeStorage::serialize( Handle name )
{
  Handle new_name = local_to_storage( name );
  string file_name = base64::encode( new_name );
  const filesystem::path dir { FIX_DIR };
  ofstream output_file( dir / file_name );

  switch ( new_name.get_content_type() ) {
    case ContentType::Blob: {
      output_file << user_get_blob( new_name );
      output_file.close();
      return file_name;
    }

    case ContentType::Tree: {
      span_view<Handle> tree = get_tree( new_name );
      for ( size_t i = 0; i < tree.size(); i++ ) {
        serialize( tree[i] );
      }
      for ( size_t i = 0; i < tree.size(); i++ ) {
        output_file << base64::encode( tree[i] );
      }
      output_file.close();

      return file_name;
    }

    case ContentType::Thunk: {
      output_file << base64::encode( Handle::get_encode_name( name ) );
      return file_name;
    }

    default:
      throw runtime_error( "Unknown content type." );
  }

  return file_name;
}

void RuntimeStorage::deserialize()
{
  const filesystem::path dir { FIX_DIR };

  for ( const auto& file : filesystem::directory_iterator( dir ) ) {
    Handle name( base64::decode( file.path().filename().string() ) );

    if ( name.is_literal_blob() || name.is_local() || fix_cache_.contains( name ) ) {
      continue;
    }

    ifstream input_file( file.path() );
    if ( !input_file.is_open() ) {
      throw runtime_error( "Serialized file does not exist." );
    }

    switch ( name.get_content_type() ) {
      case ContentType::Thunk: {
        continue;
      }

      case ContentType::Blob: {
        input_file.seekg( 0, std::ios::end );
        size_t size = input_file.tellg();
        char* buf = static_cast<char*>( malloc( size ) );
        input_file.seekg( 0, std::ios::beg );
        input_file.read( buf, size );

        Blob blob( Blob_ptr( buf ), size );
        Handle local_id = add_blob( std::move( blob ) );
        fix_cache_.insert_or_assign( name, local_id );

        continue;
      }

      case ContentType::Tree: {
        input_file.seekg( 0, std::ios::end );
        size_t size = input_file.tellg();
        char* buf = static_cast<char*>( malloc( size ) );
        Handle* tree_buf = static_cast<Handle*>( aligned_alloc( alignof( Handle ), sizeof( Handle ) * size / 43 ) );
        input_file.seekg( 0, std::ios::beg );
        input_file.read( reinterpret_cast<char*>( buf ), size );

        for ( size_t i = 0; i < size; i += 43 ) {
          tree_buf[i / 43] = base64::decode( string( buf + i, 43 ) );
        }

        Tree tree( Tree_ptr( tree_buf ), size / 43 );
        free( buf );

        Handle local_id = add_tree( std::move( tree ) );
        fix_cache_.insert_or_assign( name, local_id );

        continue;
      }

      case ContentType::Tag: {
        input_file.seekg( 0, std::ios::end );
        size_t size = input_file.tellg();
        char* buf = static_cast<char*>( malloc( size ) );
        Handle* tree_buf = static_cast<Handle*>( aligned_alloc( alignof( Handle ), sizeof( Handle ) * size / 43 ) );
        input_file.seekg( 0, std::ios::beg );
        input_file.read( reinterpret_cast<char*>( buf ), size );

        for ( size_t i = 0; i < size; i += 43 ) {
          tree_buf[i / 43] = base64::decode( string( buf + i, 43 ) );
        }

        Tree tree( Tree_ptr( tree_buf ), size / 43 );
        free( buf );

        Handle local_id = add_tag( std::move( tree ) );
        fix_cache_.insert_or_assign( name, local_id );

        continue;
      }

      default:
        continue;
    }
  }
}
