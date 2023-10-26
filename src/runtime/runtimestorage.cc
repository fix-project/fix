#include "base64.hh"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <shared_mutex>
#include <stdexcept>
#include <type_traits>

#include <glog/logging.h>

#include "handle.hh"
#include "object.hh"
#include "operation.hh"
#include "runtimestorage.hh"
#include "sha256.hh"
#include "spans.hh"
#include "task.hh"
#include "wasm-rt-content.h"

using namespace std;

template<mutable_object T>
T RuntimeStorage::get( Handle name )
{
  using owned = Owned<T>;

  shared_lock lock( storage_mutex_ );
  assert( name.is_local() );

  auto& entry = local_storage_.at( name.get_local_id() );
  auto& object = std::get<OwnedMutObject>( entry );

  return std::get<owned>( object );

  /* if ( name.is_canonical() ) { */
  /*   return std::get<O>( canonical_storage_.at( name ) ); */
  /* } else if ( name.is_local() ) { */
  /*   if ( holds_alternative<Handle>( entry ) ) { */
  /*     return std::get<T>( canonical_storage_.at( std::get<Handle>( entry ) ) ); */
  /*   } else { */
  /*     auto& object = std::get<OwnedMutObject>( entry ); */
  /*     return std::get<T>( object ); */
  /*   } */
  /* } */

  /* throw out_of_range( "Invalid handle." ); */
}

template<object T>
T RuntimeStorage::get( Handle name )
{
  using owned = Owned<T>;
  using owned_mut = Owned<typename owned::mutable_span>;
  shared_lock lock( storage_mutex_ );

  assert( name.is_local() and name.is_canonical() );
  if ( name.is_canonical() ) {
    return std::get<owned>( canonical_storage_.at( name ) );
  } else {
    auto& entry = local_storage_.at( name.get_local_id() );
    if ( std::holds_alternative<Handle>( entry ) ) {
      return std::get<owned>( canonical_storage_.at( std::get<Handle>( entry ) ) );
    }
    auto& object = std::get<OwnedMutObject>( entry );
    owned_mut& object_mut = std::get<owned_mut>( object );
    return object_mut;
  }
}

Handle RuntimeStorage::add_blob( OwnedMutBlob&& blob )
{
  unique_lock lock( storage_mutex_ );
  size_t local_id = local_storage_.size();
  size_t size = blob.size();
  local_storage_.push_back( std::move( blob ) );
  return Handle( local_id, size, ContentType::Blob );
}

Handle RuntimeStorage::add_tree( OwnedMutTree&& tree )
{
  unique_lock lock( storage_mutex_ );
  size_t local_id = local_storage_.size();
  size_t size = tree.size();
  local_storage_.push_back( std::move( tree ) );
  return Handle( local_id, size, ContentType::Tree );
}

Handle RuntimeStorage::add_blob( OwnedBlob&& blob )
{
  Handle handle( sha256::encode_span( Blob( blob ) ), blob.size(), ContentType::Blob );
  {
    unique_lock lock( storage_mutex_ );
    if ( not canonical_storage_.contains( handle ) ) {
      canonical_storage_.insert_or_assign( handle, std::move( blob ) );
    }
  }
  return handle;
}

Handle RuntimeStorage::add_tree( OwnedTree&& tree )
{
  Handle handle( sha256::encode_span( Tree( tree ) ), tree.size(), ContentType::Tree );
  {
    unique_lock lock( storage_mutex_ );
    if ( not canonical_storage_.contains( handle ) ) {
      canonical_storage_.insert_or_assign( handle, std::move( tree ) );
    }
  }
  return handle;
}

Blob RuntimeStorage::get_blob( Handle name )
{
  assert( name.is_blob() );
  if ( name.is_literal_blob() ) {
    return name.literal_blob();
  } else {
    return get<Blob>( name );
  }

  throw out_of_range( "Blob does not exist." );
}

Tree RuntimeStorage::get_tree( Handle name )
{
  assert( not name.is_blob() );
  return get<Tree>( name );
}

std::list<Task> RuntimeStorage::get_local_tasks( Task task )
{
  if ( task.handle().is_local() ) {
    return {};
  }

  shared_lock lock( storage_mutex_ );
  if ( canonical_to_local_cache_for_tasks_.contains( task.handle() ) ) {
    auto local_handles = canonical_to_local_cache_for_tasks_.at( task.handle() );
    std::list<Task> result;
    for ( const auto& handle : local_handles ) {
      result.push_back( Task( handle, task.operation() ) );
    }

    return result;
  }

  return {};
}

Handle RuntimeStorage::canonicalize( Handle handle )
{
  VLOG( 1 ) << "canonicalizing " << handle;
  if ( handle.is_canonical() ) {
    return handle;
  }

  Laziness laziness = handle.get_laziness();
  Handle name = Handle::name_only( handle );

  {
    shared_lock lock( storage_mutex_ );
    if ( holds_alternative<Handle>( local_storage_.at( name.get_local_id() ) ) ) {
      return std::get<Handle>( local_storage_.at( name.get_local_id() ) ).with_laziness( laziness );
    }
  }

  switch ( name.get_content_type() ) {
    case ContentType::Blob: {
      auto blob = get<Blob>( name );
      Handle hash( sha256::encode_span( blob ), blob.size(), ContentType::Blob );

      {
        unique_lock lock( storage_mutex_ );
        std::variant<OwnedMutObject, Handle> replacement = hash;
        std::swap( replacement, local_storage_.at( name.get_local_id() ) );
        OwnedMutObject mut_object = std::move( std::get<OwnedMutObject>( replacement ) );

        if ( !canonical_storage_.contains( hash ) ) {
          OwnedObject object = std::move( std::get<OwnedMutBlob>( mut_object ) );
          canonical_storage_.insert_or_assign( hash, std::move( object ) );
        }

        return hash.with_laziness( laziness );
      }

      break;
    }

    case ContentType::Tree: {
      auto orig_tree = get<Tree>( name );

      auto new_tree = OwnedMutTree::allocate( orig_tree.size() );

      for ( size_t i = 0; i < new_tree.size(); ++i ) {
        new_tree[i] = canonicalize( orig_tree[i] );
      }

      Handle hash( sha256::encode_span( Tree( new_tree ) ), new_tree.size(), name.get_content_type() );

      {
        unique_lock lock( storage_mutex_ );
        if ( !canonical_storage_.contains( hash ) ) {
          OwnedTree new_tree_const( std::move( new_tree ) );
          canonical_storage_.insert_or_assign( hash, std::move( new_tree_const ) );
        }

        // NOTE: this will implicitly free the memory of the non-canonicalized tree
        local_storage_.at( name.get_local_id() ) = hash;

        return hash.with_laziness( laziness );
      }

      break;
    }

    case ContentType::Tag: {
      return canonicalize( handle.as_tree() ).as_tag();
    }

    case ContentType::Thunk: {
      return canonicalize( handle.as_tree() ).as_thunk();
    }

    default:
      throw runtime_error( "Unknown content type." );
  }
}

Task RuntimeStorage::canonicalize( Task task )
{
  VLOG( 1 ) << "canonicalizing " << task;
  if ( task.handle().is_canonical() ) {
    return task;
  } else {
    Task canonical_task( canonicalize( task.handle() ), task.operation() );
    {
      shared_lock lock( storage_mutex_ );
      canonical_to_local_cache_for_tasks_[canonical_task.handle()].push_back( task.handle() );
    }
    return canonical_task;
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

void RuntimeStorage::serialize( Handle name )
{
  const auto fix_dir = get_fix_repo();
  Handle storage = canonicalize( name );
  for ( const auto& ref : get_friendly_names( name ) ) {
    ofstream output_file { fix_dir / "refs" / ref, std::ios::trunc };
    output_file << base64::encode( storage );
  }
  serialize_objects( name, fix_dir / "objects" );
}

void RuntimeStorage::serialize_objects( Handle name, const filesystem::path& dir )
{
  Handle new_name = canonicalize( name );
  string file_name = base64::encode( new_name );

  switch ( new_name.get_content_type() ) {
    case ContentType::Blob: {
      ofstream output_file( dir / file_name );
      Blob blob = get_blob( new_name );
      output_file.write( blob.data(), blob.size() );
      output_file.close();
      return;
    }

    case ContentType::Tree: {
      ofstream output_file( dir / file_name );
      Tree tree = get_tree( new_name );
      for ( size_t i = 0; i < tree.size(); i++ ) {
        serialize_objects( tree[i], dir );
      }
      for ( size_t i = 0; i < tree.size(); i++ ) {
        output_file << base64::encode( tree[i] );
      }
      output_file.close();
      return;
    }

    case ContentType::Tag:
    case ContentType::Thunk: {
      Handle tree = name.as_tree();
      serialize_objects( tree, dir );
      return;
    }

    default:
      throw runtime_error( "Unknown content type." );
  }
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
    VLOG( 2 ) << "deserializing " << file << " to " << name;

    if ( name.is_local() ) {
      throw runtime_error( "Attempted to deserialize a local name." );
    }
    if ( name.is_literal_blob() ) {
      continue;
    }

    switch ( name.get_content_type() ) {
      case ContentType::Blob: {
        canonicalize( add_blob( OwnedBlob::from_file( file ) ) );
        break;
      }

      case ContentType::Tree: {
        canonicalize( add_tree( OwnedTree::from_file( file ) ) );
      }

      case ContentType::Thunk:
      case ContentType::Tag: {
        break;
      }

      default:
        break;
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

  shared_lock lock( storage_mutex_ );

  if ( handle.is_thunk() ) {
    return RuntimeStorage::contains( handle.as_tree() );
  }
  switch ( handle.get_laziness() ) {
    case Laziness::Lazy:
      return true;
    case Laziness::Shallow:
      if ( handle.is_canonical() ) {
        return canonical_storage_.contains( handle );
      } else {
        return local_storage_.size() > handle.get_local_id();
      }
    case Laziness::Strict:
      if ( handle.is_canonical() ) {
        return canonical_storage_.contains( handle.as_strict() );
      } else {
        return local_storage_.size() > handle.get_local_id();
      }
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
        visit( handle.as_tree(), visitor );
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
      return complete( handle.as_tree() );
    case ContentType::Tree:
    case ContentType::Tag: {
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
