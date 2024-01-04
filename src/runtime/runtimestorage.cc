#include <filesystem>
#include <fstream>
#include <iostream>
#include <shared_mutex>
#include <stdexcept>
#include <type_traits>

#include <glog/logging.h>
#include <variant>

#include "base16.hh"
#include "handle.hh"
#include "object.hh"
#include "operation.hh"
#include "relation.hh"
#include "runtimestorage.hh"
#include "sha256.hh"
#include "spans.hh"
#include "task.hh"
#include "util.hh"
#include "wasm-rt-content.h"

using namespace std;

template<mutable_object T>
T RuntimeStorage::get( Handle name )
{
  using owned = Owned<T>;

  CHECK( name.is_local() );

  auto& entry = local_storage_.at( name.get_local_id() );
  auto& object = std::get<OwnedMutObject>( entry );

  return std::get<owned>( object );
}

template<object T>
T RuntimeStorage::get( Handle name )
{
  using owned = Owned<T>;
  using owned_mut = Owned<typename owned::mutable_span>;

  if constexpr ( std::is_same_v<T, Blob> ) {
    if ( name.is_literal_blob() ) {
      return name.literal_blob();
    }
  }

  CHECK( name.is_local() or name.is_canonical() );
  if ( name.is_canonical() ) {
    VLOG( 2 ) << "get " << name << ": canonical";
    return std::get<owned>( canonical_storage_.at( name ) );
  } else {
    VLOG( 2 ) << "get " << name << ": local @ " << name.get_local_id();
    auto& entry = local_storage_.at( name.get_local_id() );
    if ( std::holds_alternative<Handle>( entry ) ) {
      VLOG( 2 ) << "get " << name << " -> canonical @ " << std::get<Handle>( entry );
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
  if ( blob.size() < 32 ) {
    return Handle( { blob.data(), blob.size() } );
  }
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

Handle RuntimeStorage::add_blob( OwnedBlob&& blob, optional<Handle> name )
{
  Handle handle;
  if ( name ) {
    handle = *name;
  } else {
    if ( blob.size() < 32 ) {
      handle = Handle( { blob.data(), blob.size() } );
    } else {
      handle = Handle( sha256::encode_span( Blob( blob ) ), blob.size(), ContentType::Blob );
    }
  }
  {
    unique_lock lock( storage_mutex_ );
    canonical_storage_.insert( { handle, std::move( blob ) } );
  }
  return handle;
}

Handle RuntimeStorage::add_tree( OwnedTree&& tree, optional<Handle> name )
{
  Handle handle;
  if ( name ) {
    handle = *name;
  } else {
    handle = Handle( sha256::encode_span( Tree( tree ) ), tree.size(), ContentType::Blob );
  }
  {
    unique_lock lock( storage_mutex_ );
    canonical_storage_.insert( { handle, std::move( tree ) } );
  }
  return handle;
}

Blob RuntimeStorage::get_blob( const Handle& name )
{
  VLOG( 2 ) << "get_blob " << name;
  CHECK( name.is_blob() );
  if ( name.is_literal_blob() ) {
    return name.literal_blob();
  } else {
    shared_lock lock( storage_mutex_ );
    return get<Blob>( name );
  }

  throw out_of_range( "Blob does not exist." );
}

Tree RuntimeStorage::get_tree( Handle name )
{
  VLOG( 2 ) << "get_tree " << name;
  CHECK( not name.is_blob() );
  shared_lock lock( storage_mutex_ );
  return get<Tree>( name.as_tree() );
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
  unique_lock<shared_mutex> lock( storage_mutex_ );
  return canonicalize( handle, lock );
}

Handle RuntimeStorage::canonicalize( Handle handle, unique_lock<shared_mutex>& lock )
{
  VLOG( 1 ) << "canonicalizing " << handle;
  if ( handle.is_canonical() ) {
    VLOG( 2 ) << "already canonical";
    return handle;
  }

  Laziness laziness = handle.get_laziness();
  Handle name = Handle::name_only( handle );

  if ( name.is_literal_blob() ) {
    return name;
  }

  if ( holds_alternative<Handle>( local_storage_.at( name.get_local_id() ) ) ) {
    VLOG( 2 ) << "using gravestone";
    switch ( handle.get_content_type() ) {
      case ContentType::Blob:
      case ContentType::Tree:
        return std::get<Handle>( local_storage_.at( name.get_local_id() ) ).with_laziness( laziness );

      case ContentType::Tag:
        return std::get<Handle>( local_storage_.at( name.get_local_id() ) ).with_laziness( laziness ).as_tag();

      case ContentType::Thunk:
        return std::get<Handle>( local_storage_.at( name.get_local_id() ) ).with_laziness( laziness ).as_thunk();
    }
  }

  switch ( name.get_content_type() ) {
    case ContentType::Blob: {
      VLOG( 2 ) << "canonicalizing blob";
      auto blob = get<Blob>( name );
      if ( blob.size() < 32 ) {
        return Handle( std::string_view( blob.data(), blob.size() ) );
      }

      Handle hash( sha256::encode_span( blob ), blob.size(), ContentType::Blob );

      std::variant<OwnedMutObject, Handle> replacement = hash;
      std::swap( replacement, local_storage_.at( name.get_local_id() ) );
      CHECK( holds_alternative<OwnedMutObject>( replacement ) );
      OwnedMutObject mut_object = std::move( std::get<OwnedMutObject>( replacement ) );

      CHECK( holds_alternative<OwnedMutBlob>( mut_object ) );
      OwnedObject object = std::move( std::get<OwnedMutBlob>( mut_object ) );
      canonical_storage_.insert_or_assign( hash, std::move( object ) );

      return hash.with_laziness( laziness );

      break;
    }

    case ContentType::Tree: {
      VLOG( 2 ) << "canonicalizing tree";
      auto orig_tree = get<Tree>( name );

      auto new_tree = OwnedMutTree::allocate( orig_tree.size() );

      for ( size_t i = 0; i < new_tree.size(); ++i ) {
        new_tree[i] = canonicalize( orig_tree[i], lock );
      }

      Handle hash( sha256::encode_span( Tree( new_tree ) ), new_tree.size(), name.get_content_type() );

      OwnedTree new_tree_const( std::move( new_tree ) );
      canonical_storage_.insert_or_assign( hash, std::move( new_tree_const ) );

      // NOTE: this will implicitly free the memory of the non-canonicalized tree
      local_storage_.at( name.get_local_id() ) = hash;

      return hash.with_laziness( laziness );
    }

    case ContentType::Tag: {
      VLOG( 2 ) << "canonicalizing tag";
      return canonicalize( handle.as_tree(), lock ).as_tag();
    }

    case ContentType::Thunk: {
      VLOG( 2 ) << "canonicalizing thunk";
      return canonicalize( handle.as_tree(), lock ).as_thunk();
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

void RuntimeStorage::serialize_object( Handle name, const filesystem::path& dir )
{
  VLOG( 1 ) << "Serializing " << name;
  Handle new_name = canonicalize( name );

  for ( const auto& ref : get_friendly_names( new_name ) ) {
    ofstream output_file { get_fix_repo() / "refs" / ref, std::ios::trunc };
    output_file << base16::encode( new_name );
  }

  if ( new_name.is_literal_blob() )
    return;
  string file_name = base16::encode( new_name );

  switch ( new_name.get_content_type() ) {
    case ContentType::Blob:
    case ContentType::Tree: {
      if ( not std::filesystem::exists( dir / file_name ) )
        std::visit( [&]( auto& arg ) -> void { arg.to_file( dir / file_name ); },
                    canonical_storage_.at( new_name ) );

      return;
    }

    case ContentType::Tag: {
      if ( not std::filesystem::exists( dir / file_name ) ) {
        string tree_file_name = base16::encode( new_name.as_tree() );
        filesystem::create_symlink( dir / tree_file_name, dir / file_name );
      }
      return;
    }

    case ContentType::Thunk: {
      return;
    }

    default:
      throw runtime_error( "Unknown content type." );
  }
}

void RuntimeStorage::serialize_relation( Relation relation )
{
  VLOG( 1 ) << "Serializing " << relation << endl;
  if ( relation.type() == RelationType::Eval and relation.lhs().get_content_type() == ContentType::Blob )
    return;

  const auto fix_dir = get_fix_repo();
  Handle lhs_handle = canonicalize( relation.lhs() );
  Handle rhs_handle = canonicalize( relation.rhs() );

  string file_name = base16::encode( lhs_handle );

  filesystem::path path = fix_dir / "relations";
  if ( not std::filesystem::exists( path ) ) {
    std::filesystem::create_directory( path );
  }

  path = path / RELATIONTYPE_NAMES[to_underlying( relation.type() )];
  if ( not std::filesystem::exists( path ) ) {
    std::filesystem::create_directory( path );
  }

  path = path / file_name;
  if ( not std::filesystem::exists( path ) ) {
    ofstream output_file( path );
    output_file << string_view { reinterpret_cast<char*>( &rhs_handle ), sizeof( Handle ) };
  }
}

void RuntimeStorage::serialize_pin_relations( Handle src, const unordered_set<Handle>& dsts )
{
  if ( dsts.empty() )
    return;

  const auto fix_dir = get_fix_repo();
  Handle lhs_handle = canonicalize( src );
  string file_name = base16::encode( lhs_handle );

  filesystem::path path = fix_dir / "relations";
  if ( not std::filesystem::exists( path ) ) {
    std::filesystem::create_directory( path );
  }

  path = path / "Pin";
  if ( not std::filesystem::exists( path ) ) {
    std::filesystem::create_directory( path );
  }

  path = path / file_name;
  if ( not std::filesystem::exists( path ) ) {
    ofstream output_file( path );
    for ( const auto& dst : dsts ) {
      auto canonical_dst = canonicalize( dst );
      output_file << string_view { reinterpret_cast<const char*>( &canonical_dst ), sizeof( Handle ) };
    }
  }
}

Handle read_handle( filesystem::path file )
{
  ifstream input_file( file );
  if ( !input_file.is_open() ) {
    throw runtime_error( "File does not exist: " + file.string() );
  }
  input_file.seekg( 0, std::ios::end );
  size_t size = input_file.tellg();
  if ( size != 64 ) {
    throw runtime_error( "File was an invalid size: " + file.string() + ": " + to_string( size ) );
  }
  input_file.seekg( 0, std::ios::beg );
  char buf[64];
  input_file.read( buf, size );
  Handle h = base16::decode( { buf, 64 } );
  VLOG( 1 ) << "decoded " << string_view( buf, 64 ) << " into " << h;
  return h;
}

void RuntimeStorage::deserialize()
{
  const auto fix_dir = get_fix_repo();
  for ( const auto& file : filesystem::directory_iterator( fix_dir / "refs" ) ) {
    auto handle = read_handle( file );
    friendly_names_.insert( make_pair( handle, file.path().filename() ) );
  }
  deserialize_objects( fix_dir / "objects" );
}

void RuntimeStorage::deserialize_objects( const filesystem::path& dir )
{
  for ( const auto& file : filesystem::directory_iterator( dir ) ) {
    if ( file.is_directory() )
      continue;
    Handle name( base16::decode( file.path().filename().string() ) );
    VLOG( 2 ) << "deserializing " << file << " to " << name;

    if ( name.is_local() ) {
      throw runtime_error( "Attempted to deserialize a local name." );
    }
    if ( name.is_literal_blob() ) {
      continue;
    }

    switch ( name.get_content_type() ) {
      case ContentType::Blob: {
        add_blob( OwnedBlob::from_file( file ), name );
        break;
      }

      case ContentType::Tree: {
        add_tree( OwnedTree::from_file( file ), name );
        break;
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

  if ( handle.is_thunk() || handle.is_tag() ) {
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
  return base16::encode( handle );
}

string RuntimeStorage::get_short_name( Handle handle )
{
  if ( handle.is_canonical() ) {
    auto encoded = get_encoded_name( handle );
    return encoded.substr( 0, 6 ) + encoded[63];
  } else
    return "local[" + to_string( handle.get_local_id() ) + "]";
}

Handle RuntimeStorage::get_full_name( string short_name )
{
  assert( short_name.length() == 7 );
  string prefix = short_name.substr( 0, 6 );

  auto fix_dir = get_fix_repo();
  for ( const auto& file : filesystem::directory_iterator( fix_dir / "objects" ) ) {
    auto file_name = file.path().filename().string();
    if ( file_name.starts_with( prefix ) ) {
      Handle full = base16::decode( file_name );
      if ( file_name.ends_with( short_name[6] ) ) {
        return full;
      } else if ( base16::encode( full.as_thunk() ).ends_with( short_name[6] ) ) {
        return full.as_thunk();
      }
    }
  }

  throw runtime_error( "Full name does not exist." );
}

string RuntimeStorage::get_display_name( Handle handle )
{
  vector<string> friendly_names = get_friendly_names( handle );
  string fullname = get_short_name( handle );
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
    return read_handle( ref_file );
  }
  return {};
}

void RuntimeStorage::set_ref( string_view ref, Handle handle )
{
  friendly_names_.insert( make_pair( handle, ref ) );
}

void RuntimeStorage::visit( Handle handle, function<void( Handle )> visitor )
{
  if ( handle.is_lazy() ) {
    visitor( handle );
  } else {
    switch ( handle.get_content_type() ) {
      case ContentType::Tree:
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
      case ContentType::Tag:
        visit( handle.as_tree(), visitor );
        visitor( handle );
        break;
    }
  }
}

void RuntimeStorage::visit_full( Handle handle,
                                 function<void( Handle )> visitor,
                                 function<void( Handle, unordered_set<Handle> )> pin_visitor,
                                 unordered_set<Handle> visited )
{
  if ( visited.contains( handle ) ) {
    return;
  }
  visited.insert( handle );

  switch ( handle.get_content_type() ) {
    case ContentType::Tree:
      if ( contains( handle ) ) {
        for ( const auto& element : get_tree( handle ) ) {
          visit_full( element, visitor, pin_visitor, visited );
        }
      }
      visitor( handle );
      break;
    case ContentType::Blob:
      visitor( handle );
      break;
    case ContentType::Thunk:
    case ContentType::Tag:
      visit_full( handle.as_tree(), visitor, pin_visitor, visited );
      visitor( handle );
      break;
  }

  auto pinned = get_pinned_handles( handle );
  for ( const auto& dst : pinned ) {
    visit_full( dst, visitor, pin_visitor, visited );
    visitor( dst );
  }
  pin_visitor( handle, pinned );
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
  bool res {};
  visit( handle, [&]( Handle h ) { res |= contains( h ); } );
  return res;
}

std::vector<Handle> RuntimeStorage::minrepo( Handle handle )
{
  vector<Handle> handles {};
  visit( handle, [&]( Handle h ) { handles.push_back( h ); } );
  return handles;
}

const Program& RuntimeStorage::link( Handle handle )
{
  assert( handle.is_blob() );
  unique_lock lock( storage_mutex_ );
  if ( linked_programs_.contains( handle ) ) {
    return linked_programs_.at( handle );
  }
  Program program = link_program( get<Blob>( handle ) );
  linked_programs_.insert( { handle, std::move( program ) } );
  return linked_programs_.at( handle );
}

unordered_set<Handle> RuntimeStorage::get_pinned_handles( Handle handle )
{
  shared_lock lock( storage_mutex_ );
  return pins_[handle];
}

void RuntimeStorage::pin( Handle src, Handle dst )
{
  unique_lock lock( storage_mutex_ );
  pins_[src].insert( dst );
}

unordered_set<Handle> RuntimeStorage::get_tags( Handle handle )
{
  auto fix_dir = get_fix_repo();
  unordered_set<Handle> result;

  for ( const auto& file : filesystem::directory_iterator( fix_dir / "objects" ) ) {
    Handle name( base16::decode( file.path().filename().string() ) );
    if ( name.is_tag() ) {
      auto tree = get_tree( name );
      if ( compare_handles( tree[1], handle ) ) {
        result.insert( name );
      }
    }
  }

  return result;
}
