#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "base64.hh"

#include "object.hh"
#include "runtimestorage.hh"
#include "sha256.hh"
#include "wasm-rt-content.h"

using namespace std;

bool RuntimeStorage::steal_work( Job& job, size_t tid )
{
  for ( size_t i = 0; i <= num_workers_; ++i ) {
    if ( workers_[( i + tid ) % ( num_workers_ + 1 ) ]->jobs_.pop( job ) ) {
      return true;
    }
  }
  return false;
}

Name RuntimeStorage::force_thunk( Name name )
{
  return workers_.at( 0 )->force_thunk( name );
}

Name RuntimeStorage::add_blob( Blob&& blob )
{
  if ( blob.size() > 31 ) {
    size_t local_id = local_storage_.push_back( move( blob ) );
    Name name( local_id, blob.size(), ContentType::Blob );
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
  size_t local_id = local_storage_.push_back( move( tree ) );
  Name name( local_id, tree.size(), ContentType::Tree );
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

Name RuntimeStorage::local_to_storage( Name name )
{
  if ( name.is_literal_blob() || !name.is_local() ) {
    return name;
  }

  switch ( name.get_content_type() ) {
    case ContentType::Blob: {
      const ObjectOrName& obj = local_storage_.at( name.get_local_id() );
      if ( holds_alternative<Name>( obj ) ) {
        Name new_name = get<Name>( obj );
        assert( !new_name.is_local() );

        return new_name;
      } else if ( holds_alternative<Blob>( obj ) ) {
        string_view blob = get<Blob>( obj );
        Name new_name( sha256::encode( blob ), blob.size(), name.get_metadata() );
        storage.put( new_name, move( get<Blob>( local_storage_.at( name.get_local_id() ) ) ) );
        local_storage_.at( name.get_local_id() ) = new_name;

        return new_name;
      } else {
        throw runtime_error( "Name type does not match content type" );
      }
      break;
    }

    case ContentType::Tree: {
      const ObjectOrName& obj = local_storage_.at( name.get_local_id() );
      if ( holds_alternative<Name>( obj ) ) {
        Name new_name = get<Name>( obj );
        assert( !new_name.is_local() );
        return new_name;
      } else if ( holds_alternative<Tree>( obj ) ) {
        span_view<Name> orig_tree = get<Tree>( obj );

        for ( size_t i = 0; i < orig_tree.size(); ++i ) {
          auto entry = orig_tree[i];
          orig_tree.mutable_data()[i] = local_to_storage( entry );
        }

        string_view view( reinterpret_cast<char*>( orig_tree.mutable_data() ), orig_tree.size() * sizeof( Name ) );
        Name new_name( sha256::encode( view ), orig_tree.size(), name.get_metadata() );
        storage.put( new_name, move( get<Tree>( local_storage_.at( name.get_local_id() ) ) ) );
        local_storage_.at( name.get_local_id() ) = new_name;

        return new_name;

      } else {
        throw runtime_error( "Name type does not match content type" );
      }
      break;
    }

    case ContentType::Thunk: {
      return Name::get_thunk_name( local_to_storage( Name::get_encode_name( name ) ) );
      break;
    }

    default:
      throw runtime_error( "Unknown content type." );
  }
}

string RuntimeStorage::serialize( Name name )
{
  Name new_name = local_to_storage( name );
  string file_name = base64::encode( new_name );
  ofstream output_file( ".fix/" + file_name );

  switch ( new_name.get_content_type() ) {
    case ContentType::Blob: {
      output_file << user_get_blob( new_name );
      output_file.close();
      return file_name;
    }

    case ContentType::Tree: {
      span_view<Name> tree = get_tree( new_name );
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
      output_file << base64::encode( Name::get_encode_name( name ) );
      return file_name;
    }

    default:
      throw runtime_error( "Unknown content type." );
  }

  return file_name;
}

void RuntimeStorage::deserialize()
{
  const filesystem::path dir { ".fix" };

  for ( const auto& file : filesystem::directory_iterator( dir ) ) {
    Name name( base64::decode( file.path().filename().string() ) );

    if ( name.is_literal_blob() || name.is_local() || storage.contains( name ) ) {
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
        storage.put( name, move( blob ) );
        continue;
      }

      case ContentType::Tree: {
        input_file.seekg( 0, std::ios::end );
        size_t size = input_file.tellg();
        char* buf = static_cast<char*>( malloc( size ) );
        Name* tree_buf = static_cast<Name*>( aligned_alloc( alignof( Name ), sizeof( Name ) * size / 43 ) );
        input_file.seekg( 0, std::ios::beg );
        input_file.read( reinterpret_cast<char*>( buf ), size );

        for ( size_t i = 0; i < size; i += 43 ) {
          tree_buf[i / 43] = base64::decode( string( buf + i, 43 ) );
        }

        Tree tree( Tree_ptr( tree_buf ), size / 43 );
        free( buf );
        storage.put( name, move( tree ) );
        continue;
      }

      default:
        continue;
    }
  }
}
