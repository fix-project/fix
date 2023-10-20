#include "base64.hh"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "handle.hh"
#include "object.hh"
#include "operation.hh"
#include "runtimestorage.hh"
#include "sha256.hh"
#include "task.hh"
#include "wasm-rt-content.h"

using namespace std;

const size_t SIZE_OF_BASE64_ENCODED_HANDLE = 43;

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
    Handle local_name = canonical_to_local_.get( Handle( name ) );
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
    Handle local_name = canonical_to_local_.get( Handle( name ) );
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
    Handle local_name = canonical_to_local_.get( Handle( name ) );
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

Handle RuntimeStorage::canonicalize( Handle name )
{
  if ( name.is_canonical() ) {
    return name;
  }
  Laziness laziness = name.get_laziness();

  switch ( name.get_content_type() ) {
    case ContentType::Blob: {
      const Object& obj = local_storage_.at( name.get_local_id() );
      if ( holds_alternative<Blob>( obj ) ) {
        string_view blob = get<Blob>( obj );

        Handle hash( sha256::encode( blob ), blob.size(), ContentType::Blob );
        canonical_to_local_.insert_or_assign( hash, name );

        return hash.with_laziness( laziness );
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
          tree.mutable_data()[i] = canonicalize( entry );
        }

        string_view view( reinterpret_cast<char*>( tree.mutable_data() ), tree.size() * sizeof( Handle ) );
        Handle hash( sha256::encode( view ), tree.size(), name.get_content_type() );

        canonical_to_local_.insert_or_assign( hash, new_name );

        return hash.with_laziness( laziness );
      } else {
        throw runtime_error( "Handle type does not match content type" );
      }
      break;
    }

    case ContentType::Thunk: {
      Handle new_name = Handle::get_thunk_name( canonicalize( Handle::get_encode_name( name ) ) );

      canonical_to_local_.insert_or_assign( new_name, name );
      return new_name.with_laziness( laziness );
    }

    default:
      throw runtime_error( "Unknown content type." );
  }
}

filesystem::path RuntimeStorage::get_fix_repo()
{
  auto current_directory = filesystem::current_path();
  while ( not filesystem::exists( current_directory / ".fix" ) ) {
    if ( not current_directory.has_parent_path() ) {
      throw runtime_error( "Could not find .fix directory in any parent of the current directory." );
    }
    current_directory = current_directory.parent_path();
  }
  return current_directory / ".fix";
}

optional<Handle> RuntimeStorage::get_local_name( Handle name )
{
  if ( !canonical_to_local_.contains( name ) ) {
    return {};
  } else {
    return canonical_to_local_.get( name );
  }
}

string RuntimeStorage::serialize( Handle name )
{
  const auto fix_dir = get_fix_repo();
  Handle storage = canonicalize( name );
  for ( const auto& ref : get_friendly_names( name ) ) {
    ofstream output_file { fix_dir / "refs" / ref, std::ios::trunc };
    output_file << base64::encode( storage );
  }
  return serialize_objects( name, fix_dir / "objects" );
}

string RuntimeStorage::serialize_objects( Handle name, const filesystem::path& dir )
{
  Handle new_name = canonicalize( name );
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
        serialize_objects( tree[i], dir );
      }
      for ( size_t i = 0; i < tree.size(); i++ ) {
        output_file << base64::encode( tree[i] );
      }
      output_file.close();

      return file_name;
    }

    case ContentType::Thunk: {
      serialize_objects( Handle::get_encode_name( name ), dir );
      output_file << base64::encode( Handle::get_encode_name( name ) );
      return file_name;
    }

    default:
      throw runtime_error( "Unknown content type." );
  }

  return file_name;
}

vector<Handle> read_handles( filesystem::path file )
{
  ifstream input_file( file );
  if ( !input_file.is_open() ) {
    throw runtime_error( "File does not exist: " + file.string() );
  }
  input_file.seekg( 0, std::ios::end );
  size_t size = input_file.tellg();
  const size_t ENCODED_HANDLE_SIZE = 43;
  if ( size % ENCODED_HANDLE_SIZE != 0 ) {
    throw runtime_error( "File was an invalid size: " + file.string() + ": " + to_string( size ) );
  }
  size_t len = size / ENCODED_HANDLE_SIZE;
  char* buf = static_cast<char*>( alloca( ENCODED_HANDLE_SIZE ) );
  input_file.seekg( 0, std::ios::beg );
  vector<Handle> handles;
  for ( size_t i = 0; i < len; i++ ) {
    input_file.read( buf, size );
    handles.push_back( base64::decode( string_view( buf, ENCODED_HANDLE_SIZE ) ) );
  }
  return handles;
}

void RuntimeStorage::deserialize()
{
  const auto fix_dir = get_fix_repo();
  for ( const auto& file : filesystem::directory_iterator( fix_dir / "refs" ) ) {
    auto handles = read_handles( file );
    friendly_names_.insert( make_pair( handles.at( 0 ), file.path().filename() ) );
  }
  deserialize_objects( fix_dir / "objects" );
}

void RuntimeStorage::deserialize_objects( const filesystem::path& dir )
{
  for ( const auto& file : filesystem::directory_iterator( dir ) ) {
    if ( file.is_directory() )
      continue;
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
        assert( size == SIZE_OF_BASE64_ENCODED_HANDLE );
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
        Handle* tree_buf = static_cast<Handle*>(
          aligned_alloc( alignof( Handle ), sizeof( Handle ) * size / SIZE_OF_BASE64_ENCODED_HANDLE ) );
        input_file.seekg( 0, std::ios::beg );
        input_file.read( reinterpret_cast<char*>( buf ), size );

        for ( size_t i = 0; i < size; i += SIZE_OF_BASE64_ENCODED_HANDLE ) {
          tree_buf[i / SIZE_OF_BASE64_ENCODED_HANDLE]
            = base64::decode( string( buf + i, SIZE_OF_BASE64_ENCODED_HANDLE ) );
        }

        Tree tree( Tree_ptr( tree_buf ), size / SIZE_OF_BASE64_ENCODED_HANDLE );
        free( buf );

        Handle local_id = add_tree( std::move( tree ) );
        canonical_to_local_.insert_or_assign( name, local_id );

        continue;
      }

      case ContentType::Tag: {
        input_file.seekg( 0, std::ios::end );
        size_t size = input_file.tellg();
        char* buf = static_cast<char*>( malloc( size ) );
        Handle* tree_buf = static_cast<Handle*>(
          aligned_alloc( alignof( Handle ), sizeof( Handle ) * size / SIZE_OF_BASE64_ENCODED_HANDLE ) );
        input_file.seekg( 0, std::ios::beg );
        input_file.read( reinterpret_cast<char*>( buf ), size );

        for ( size_t i = 0; i < size; i += SIZE_OF_BASE64_ENCODED_HANDLE ) {
          tree_buf[i / SIZE_OF_BASE64_ENCODED_HANDLE]
            = base64::decode( string( buf + i, SIZE_OF_BASE64_ENCODED_HANDLE ) );
        }

        Tree tree( Tree_ptr( tree_buf ), size / SIZE_OF_BASE64_ENCODED_HANDLE );
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

bool RuntimeStorage::contains( Handle handle )
{
  if ( handle.is_lazy() ) {
    return true;
  }
  if ( handle.is_literal_blob() ) {
    return true;
  }
  if ( handle.is_thunk() ) {
    return RuntimeStorage::contains( handle.get_encode_name() );
  }
  switch ( handle.get_laziness() ) {
    case Laziness::Lazy:
      return true;
    case Laziness::Shallow:
      if ( handle.is_canonical() ) {
        if ( not canonical_to_local_.contains( handle.as_strict() ) ) {
          return false;
        }
        handle = canonical_to_local_.get( handle.as_strict() );
      }
      return local_storage_.size() > handle.get_local_id();
    case Laziness::Strict:
      if ( handle.is_canonical() ) {
        if ( not canonical_to_local_.contains( handle.as_strict() ) ) {
          return false;
        }
        handle = canonical_to_local_.get( handle.as_strict() );
      }
      return local_storage_.size() > handle.get_local_id();
  }
  return false;
}

vector<string> RuntimeStorage::get_friendly_names( Handle handle )
{
  handle = handle.as_strict();
  vector<string> names;
  const auto [begin, end] = friendly_names_.equal_range( handle );
  for ( auto it = begin; it != end; it++ ) {
    auto x = *it;
    names.push_back( x.second );
  }
  return names;
}

string RuntimeStorage::get_encoded_name( Handle handle )
{
  handle = handle.as_strict();
  return base64::encode( handle );
}

string RuntimeStorage::get_short_name( Handle handle )
{
  if ( handle.is_canonical() )
    return get_encoded_name( handle ).substr( 0, 7 );
  else
    return "local[" + to_string( handle.get_local_id() ) + "]";
}

string RuntimeStorage::get_display_name( Handle handle )
{
  string fullname = get_short_name( handle );
  vector<string> friendly_names = get_friendly_names( handle );
  if ( not friendly_names.empty() ) {
    fullname += " (";
    for ( const auto& name : friendly_names ) {
      fullname += name + ", ";
    }
    fullname = fullname.substr( 0, fullname.size() - 2 );
    fullname += ")";
  }
  return fullname;
}

optional<Handle> RuntimeStorage::get_ref( string_view ref )
{
  auto fix = get_fix_repo();
  auto ref_file = fix / "refs" / ref;
  if ( filesystem::exists( ref_file ) ) {
    return read_handles( ref_file )[0];
  }
  return {};
}

void RuntimeStorage::set_ref( string_view ref, Handle handle )
{
  friendly_names_.insert( make_pair( handle, ref ) );
}

void RuntimeStorage::visit( Handle handle, function<void( Handle )> visitor )
{
  handle = canonicalize( handle );
  if ( handle.is_lazy() ) {
    visitor( handle );
  } else {
    switch ( handle.get_content_type() ) {
      case ContentType::Tree:
      case ContentType::Tag:
        if ( contains( handle ) ) {
          for ( const auto& element : get_tree( handle ) ) {
            visit( handle.is_shallow() ? element.as_lazy() : element, visitor );
          }
        }
        visitor( handle );
        break;
      case ContentType::Blob:
        visitor( handle );
        break;
      case ContentType::Thunk:
        visit( handle.get_encode_name(), visitor );
        visitor( handle );
        break;
    }
  }
}

bool RuntimeStorage::compare_handles( Handle x, Handle y )
{
  if ( x == y ) {
    return true;
  }
  if ( x.is_canonical() and y.is_canonical() ) {
    return x == y;
  } else {
    x = canonicalize( x );
    y = canonicalize( y );
    return x == y;
  }
}

bool RuntimeStorage::complete( Handle handle )
{
  if ( not contains( handle ) ) {
    return false;
  }
  if ( handle.is_lazy() ) {
    return true;
  }
  switch ( handle.get_content_type() ) {
    case ContentType::Blob:
      return contains( handle );
    case ContentType::Thunk:
      return complete( handle.get_encode_name() );
    case ContentType::Tree:
    case ContentType::Tag: {
      Tree tree = get_tree( handle );
      bool full = true;
      for ( const auto& element : get_tree( handle ) ) {
        full |= handle.is_shallow() ? contains( element ) : complete( element );
      }
      return full;
    }
  }
  __builtin_unreachable();
}

std::vector<Handle> RuntimeStorage::minrepo( Handle handle )
{
  vector<Handle> handles {};
  visit( handle, [&]( Handle h ) { handles.push_back( h ); } );
  return handles;
}
