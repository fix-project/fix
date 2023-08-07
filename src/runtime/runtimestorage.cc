#include "base64.hh"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "object.hh"
#include "operation.hh"
#include "runtimestorage.hh"
#include "sha256.hh"
#include "task.hh"
#include "wasm-rt-content.h"

using namespace std;

bool RuntimeStorage::steal_work( Task& task, size_t tid )
{
  // Starting at the next thread index after the current thread--look to steal work
  for ( size_t i = 0; i < num_workers_; ++i ) {
    if ( workers_[( i + tid + 1 ) % num_workers_]->runq_.pop( task ) ) {
      return true;
    }
  }
  return false;
}

Handle RuntimeStorage::eval_thunk( Handle name )
{
  Task task( name, Operation::Eval );
  auto cached = fix_cache_.get( task );
  if ( cached && cached.value() )
    return cached.value().value();

  fix_cache_.start( task, workers_[0].get()->queue_cb );

  return fix_cache_.get_or_block( Task( name, Operation::Eval ) );
}

Handle RuntimeStorage::add_blob( Blob&& blob )
{
  if ( blob.size() > 31 ) {
    size_t local_id = local_storage_.push_back( std::move( blob ) );
    Handle name( local_id, blob.size(), ContentType::Blob );
    return name;
  } else {
    Handle name( blob );
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
    Handle local_name = canonical_to_local_.at( Handle( name ) );
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
    Handle local_name = canonical_to_local_.at( Handle( name ) );
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
    Handle local_name = canonical_to_local_.at( Handle( name ) );
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
        canonical_to_local_.insert_or_assign( hash, name );

        return hash;
      } else {
        throw runtime_error( "Handle type does not match content type" );
      }

      break;
    }

    case ContentType::Tree:
    case ContentType::Tag: {
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

        canonical_to_local_.insert_or_assign( hash, new_name );

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
  return serialize_to_dir( name, FIX_DIR );
}

string RuntimeStorage::serialize_to_dir( Handle name, const filesystem::path& dir )
{
  Handle new_name = local_to_storage( name );
  string file_name = base64::encode( new_name );
  ofstream output_file( dir / file_name );

  switch ( new_name.get_content_type() ) {
    case ContentType::Blob: {
      output_file << user_get_blob( new_name );
      output_file.close();
      return file_name;
    }

    case ContentType::Tree:
    case ContentType::Tag: {
      span_view<Handle> tree = get_tree( new_name );
      for ( size_t i = 0; i < tree.size(); i++ ) {
        serialize_to_dir( tree[i], dir );
      }
      for ( size_t i = 0; i < tree.size(); i++ ) {
        output_file << base64::encode( tree[i] );
      }
      output_file.close();

      return file_name;
    }

    case ContentType::Thunk: {
      serialize_to_dir( Handle::get_encode_name( name ), dir );
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
  deserialize_from_dir( FIX_DIR );
}

void RuntimeStorage::deserialize_from_dir( const filesystem::path& dir )
{

  for ( const auto& file : filesystem::directory_iterator( dir ) ) {
    Handle name( base64::decode( file.path().filename().string() ) );

    if ( name.is_local() ) {
      throw runtime_error( "Attempted to deserialize a local name." );
    }
    if ( name.is_literal_blob() ) {
      continue;
    }

    ifstream input_file( file.path() );
    if ( !input_file.is_open() ) {
      throw runtime_error( "Serialized file does not exist." );
    }

    switch ( name.get_content_type() ) {
      case ContentType::Thunk: {
        input_file.seekg( 0, std::ios::end );
        size_t size = input_file.tellg();
        assert( size == 43 );
        char* buf = static_cast<char*>( malloc( size ) );
        input_file.seekg( 0, std::ios::beg );
        input_file.read( reinterpret_cast<char*>( buf ), size );

        Handle handle;
        handle = base64::decode( buf );

        Thunk thunk( handle );
        Handle local_id = add_thunk( thunk );
        canonical_to_local_.insert_or_assign( name, local_id );
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
        canonical_to_local_.insert_or_assign( name, local_id );

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
        canonical_to_local_.insert_or_assign( name, local_id );

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
        canonical_to_local_.insert_or_assign( name, local_id );

        continue;
      }

      default:
        continue;
    }
  }
}

size_t RuntimeStorage::get_total_size( Handle name )
{
  if ( name.is_lazy() ) {
    return sizeof( __m256i ); // just a name
  } else if ( name.is_shallow() ) {
    if ( name.is_blob() ) {
      return sizeof( __m256i ); // just a name (which includes the length)
    } else if ( name.is_tree() or name.is_tag() ) {
      return sizeof( __m256i ) * ( name.get_length() + 1 ); // name of current handle + names of each child
    } else if ( name.is_thunk() ) {
      return 2 * sizeof( __m256i ); // name of current handle + name of encode
    } else {
      // in case we add new Object types later
      throw std::runtime_error( "get_total_size not fully implemented for shallow handle" );
    }
  } else if ( name.is_strict() ) {
    if ( name.is_blob() ) {
      if ( name.is_literal_blob() ) {
        // just the name
        return sizeof( __m256i );
      } else {
        // name + contents
        return sizeof( __m256i ) + name.get_length();
      }
    } else if ( name.is_tree() or name.is_tag() ) {
      // name + size of all children
      size_t size = sizeof( __m256i );
      for ( const auto& child : get_tree( name ) ) {
        size += get_total_size( child );
      }
      return size;
    } else if ( name.is_thunk() ) {
      return sizeof( __m256i ) + get_total_size( get_thunk_encode_name( name ) );
    } else {
      // in case we add new Object types later
      throw std::runtime_error( "get_total_size not fully implemented for strict handle" );
    }
  } else {
    // in case we add new accessibilities later
    throw std::runtime_error( "get_total_size not fully implemented" );
  }
}
