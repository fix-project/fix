#include <fstream>
#include <iostream>
#include <shared_mutex>
#include <stdexcept>

#include <glog/logging.h>
#include <variant>

#include "base16.hh"
#include "elfloader.hh"
#include "handle.hh"
#include "object.hh"
#include "overload.hh"
#include "runtimestorage.hh"
#include "sha256.hh"
#include "wasm-rt-content.h"

using namespace std;
namespace fs = std::filesystem;

Handle<Blob> RuntimeStorage::create( OwnedBlob&& blob, std::optional<Handle<Fix>> name )
{
  unique_lock lock( storage_mutex_ );
  if ( name ) {
    auto handle = fix_extract<Blob>( *name ).value();
    std::visit(
      [&]( auto x ) {
        assert( x.size() == blob.size() );
        (void)x;
      },
      handle.get() );
    assert( not( fix_is_local( handle ) ) );
    canonical_storage_.insert( { handle, std::move( blob ) } );
    return handle;
  } else {
    if ( blob.size() < Handle<Literal>::MAXIMUM_LENGTH ) {
      auto handle = Handle<Literal>( { blob.data(), blob.size() } ).into<Blob>();
      return handle;
    } else {
      uint64_t local_name = local_storage_.size();
      auto handle = Handle<Named>( local_name, blob.size() ).into<Blob>();
      local_storage_.push_back( std::move( blob ) );
      return handle;
    }
  }
}

Handle<Fix> RuntimeStorage::create( OwnedTree&& tree, std::optional<Handle<Fix>> name )
{
  VLOG( 2 ) << "add " << tree.data() << " " << tree.size();
  FixKind kind = FixKind::Object;
  for ( size_t i = 0; i < tree.size(); i++ ) {
    if ( fix_kind( tree[i] ) > kind ) {
      kind = fix_kind( tree[i] );
    }
  }
  if ( name ) {
    auto handle = *name;
    assert( fix_size( *name ) == tree.size() );
    assert( fix_kind( *name ) == kind );
    unique_lock lock( storage_mutex_ );
    canonical_storage_.insert( { *name, std::move( tree ) } );
    return handle;
  } else {
    uint64_t local_name = local_storage_.size();
    Handle<Fix> handle;
    switch ( kind ) {
      case FixKind::Object:
        VLOG( 2 ) << "creating ObjectTree";
        handle = Handle<ObjectTree>( local_name, tree.size() );
        break;
      case FixKind::Value:
        VLOG( 2 ) << "creating ValueTree";
        handle = Handle<ValueTree>( local_name, tree.size() );
        break;
      case FixKind::Expression:
        VLOG( 2 ) << "creating ExpressionTree";
        handle = Handle<ExpressionTree>( local_name, tree.size() );
        break;
      case FixKind::Fix:
        VLOG( 2 ) << "creating FixTree";
        handle = Handle<FixTree>( local_name, tree.size() );
        break;
    }
    local_storage_.push_back( std::move( tree ) );
    VLOG( 2 ) << "add " << handle;
    return handle;
  }
}

void RuntimeStorage::create( Handle<Relation> relation, Handle<Fix> result )
{
  unique_lock lock( storage_mutex_ );
  if ( fix_is_local( relation ) ) {
    local_relations_.insert( { relation, result } );
  } else if ( not fix_is_local( result ) ) {
    LOG( WARNING ) << "attempted to relate canonical handle to local handle";
    canonical_storage_.insert( { relation, canonicalize( result, lock ) } );
  }
}

template<FixTreeType T>
Handle<T> RuntimeStorage::create_tree( OwnedTree&& tree, std::optional<Handle<Fix>> name )
{
  auto fix = create( std::move( tree ), name );
  return std::visit(
    overload {
      [&]( auto t ) -> Handle<T> {
        if constexpr ( std::constructible_from<Handle<T>, decltype( t )> ) {
          return Handle<T>( t );
        } else {
          throw std::runtime_error( "tried to create tree with incorrect type" );
        }
      },
    },
    fix_data( fix ) );
}

template Handle<Tree<Object>> RuntimeStorage::create_tree( OwnedTree&&, std::optional<Handle<Fix>> );
template Handle<Tree<Value>> RuntimeStorage::create_tree( OwnedTree&&, std::optional<Handle<Fix>> );
template Handle<Tree<Expression>> RuntimeStorage::create_tree( OwnedTree&&, std::optional<Handle<Fix>> );
template Handle<Tree<Fix>> RuntimeStorage::create_tree( OwnedTree&&, std::optional<Handle<Fix>> );

BlobSpan RuntimeStorage::get_unsync( const Handle<Blob>& handle )
{
  std::optional<Handle<Literal>> literal = handle.try_into<Literal>();
  if ( literal ) {
    return { literal->data(), literal->size() };
  }
  Handle<Named> name = handle.try_into<Named>().value();
  if ( name.is_local() ) {
    const uint64_t local_name = name.local_name();
    VLOG( 2 ) << "get " << handle << ": local @ " << local_name;
    auto& entry = local_storage_.at( local_name );
    if ( std::holds_alternative<Handle<Fix>>( entry ) ) {
      const auto other = std::get<Handle<Fix>>( entry );
      VLOG( 2 ) << "get " << handle << " -> canonical @ " << other;
      VLOG( 2 ) << canonical_storage_.at( other ).index();
      return std::get<OwnedBlob>( std::get<OwnedSpan>( canonical_storage_.at( other ) ) ).span();
    }
    auto& object = std::get<OwnedSpan>( entry );
    auto& blob = std::get<OwnedBlob>( object );
    return blob.span();
  } else {
    VLOG( 2 ) << "get " << handle << ": canonical";
    return std::get<OwnedBlob>( std::get<OwnedSpan>( canonical_storage_.at( handle ) ) ).span();
  }
}

template<FixTreeType T>
TreeSpan RuntimeStorage::get_unsync( Handle<T> handle )
{
  if ( handle.is_local() ) {
    const uint64_t local_name = handle.local_name();
    VLOG( 2 ) << "get " << handle << ": local @ " << local_name;
    auto& entry = local_storage_.at( local_name );
    if ( std::holds_alternative<Handle<Fix>>( entry ) ) {
      const auto other = std::get<Handle<Fix>>( entry );
      VLOG( 2 ) << "get " << handle << " -> canonical @ " << other;
      return std::get<OwnedTree>( std::get<OwnedSpan>( canonical_storage_.at( other ) ) ).span();
    }
    auto& object = std::get<OwnedSpan>( entry );
    auto& tree = std::get<OwnedTree>( object );
    return tree.span();
  } else {
    VLOG( 2 ) << "get " << handle << ": canonical";
    return std::get<OwnedTree>( std::get<OwnedSpan>( canonical_storage_.at( handle ) ) ).span();
  }
}

Handle<Fix> RuntimeStorage::get_unsync( Handle<Relation> handle )
{
  if ( fix_is_local( handle ) ) {
    VLOG( 2 ) << "get " << handle << ": local";
    return local_relations_.at( handle );
  } else {
    VLOG( 2 ) << "get " << handle << ": canonical";
    return std::get<Handle<Fix>>( canonical_storage_.at( handle ) );
  }
}

BlobSpan RuntimeStorage::get( const Handle<Blob>& handle )
{
  std::shared_lock lock( storage_mutex_ );
  return get_unsync( handle );
}

template<FixTreeType T>
TreeSpan RuntimeStorage::get( Handle<T> handle )
{
  std::shared_lock lock( storage_mutex_ );
  return get_unsync<T>( handle );
}

Handle<Fix> RuntimeStorage::get( Handle<Relation> handle )
{
  std::shared_lock lock( storage_mutex_ );
  return get_unsync( handle );
}

template TreeSpan RuntimeStorage::get( Handle<ObjectTree> handle );
template TreeSpan RuntimeStorage::get( Handle<ValueTree> handle );
template TreeSpan RuntimeStorage::get( Handle<ExpressionTree> handle );
template TreeSpan RuntimeStorage::get( Handle<FixTree> handle );

template<FixType T>
void RuntimeStorage::visit( Handle<T> handle,
                            std::function<void( Handle<Fix> )> visitor,
                            std::unordered_set<Handle<Fix>> visited )
{
  if ( visited.contains( handle ) )
    return;
  if constexpr ( std::same_as<T, Literal> )
    return;

  if constexpr ( Handle<T>::is_fix_sum_type ) {
    if ( not( std::same_as<T, Thunk> or std::same_as<T, ValueTreeStub> or std::same_as<T, ObjectTreeStub> ) ) {
      std::visit( [&]( const auto x ) { visit( x, visitor, visited ); }, handle.get() );
    }
    if constexpr ( std::same_as<T, Relation> ) {
      auto target = get( handle );
      std::visit( [&]( const auto x ) { visit( x, visitor, visited ); }, target.get() );
    }
  } else {
    if constexpr ( FixTreeType<T> ) {
      auto tree = get( handle );
      for ( const auto& element : tree ) {
        visit( element, visitor, visited );
      }
    }
    VLOG( 2 ) << "visiting " << handle;
    visitor( handle );
    visited.insert( handle );
  }
}

template<FixType T>
void RuntimeStorage::visit_full(
  Handle<T> handle,
  std::function<void( Handle<Fix> )> visitor,
  std::optional<std::function<void( Handle<Fix>, const std::unordered_set<Handle<Fix>>& )>> pin_visitor,
  std::unordered_set<Handle<Fix>> visited )
{
  if ( visited.contains( handle ) )
    return;

  if constexpr ( Handle<T>::is_fix_sum_type ) {
    std::visit( [&]( const auto x ) { visit_full( x, visitor, pin_visitor, visited ); }, handle.get() );
    if constexpr ( std::same_as<T, Relation> ) {
      auto target = get( handle );
      std::visit( [&]( const auto x ) { visit_full( x, visitor, pin_visitor, visited ); }, target.get() );
    }
  } else if constexpr ( not std::same_as<T, Literal> ) {
    if constexpr ( FixTreeType<T> ) {
      auto tree = get( handle );
      for ( const auto& element : tree ) {
        visit_full( element, visitor, pin_visitor, visited );
      }
    }
    VLOG( 2 ) << "full-visiting " << handle;
    visitor( handle );
    visited.insert( handle );
  }

  if ( pin_visitor ) {
    auto pinned = pins( handle );
    for ( const auto& dst : pinned ) {
      visit_full( dst, visitor, pin_visitor, visited );
      visitor( dst );
    }
    ( *pin_visitor )( handle, pinned );
  }
}

template void RuntimeStorage::visit_full(
  Handle<Fix>,
  std::function<void( Handle<Fix> )>,
  std::optional<std::function<void( Handle<Fix>, const std::unordered_set<Handle<Fix>>& )>>,
  std::unordered_set<Handle<Fix>> );

template<FixType T>
Handle<T> RuntimeStorage::canonicalize( Handle<T> handle, std::unique_lock<std::shared_mutex>& lock )
{
  if ( not fix_is_local( handle ) ) {
    return handle;
  }

  if constexpr ( not Handle<T>::is_fix_sum_type ) {
    VLOG( 2 ) << "canonicalizing " << handle;
  }

  if constexpr ( Handle<T>::is_fix_sum_type ) {
    // non-terminal types: recurse until terminal
    auto canonical
      = std::visit( [&]( const auto x ) { return Handle<T>( canonicalize( x, lock ) ); }, handle.get() );
    if constexpr ( std::constructible_from<Handle<Relation>, Handle<T>> ) {
      // relations are special; we canonicalize both sides and rewrite the local relation
      auto target = get_unsync( handle );
      auto canonical_target = canonicalize( target, lock );

      local_relations_[handle] = canonical_target;
      canonical_storage_.insert( { canonical, canonical_target } );
    }
    return canonical;

  } else if constexpr ( std::same_as<T, Named> ) {
    if ( std::holds_alternative<Handle<Fix>>( local_storage_.at( handle.local_name() ) ) ) {
      return std::get<Handle<Fix>>( local_storage_.at( handle.local_name() ) )
        .template try_into<Expression>()
        .and_then( []( auto x ) { return x.template try_into<Value>(); } )
        .and_then( []( auto x ) { return x.template try_into<Object>(); } )
        .and_then( []( auto x ) { return x.template try_into<Blob>(); } )
        .and_then( []( auto x ) { return x.template try_into<Named>(); } )
        .value();
    }
    // Named Blobs: hash the contents
    auto span = get_unsync( handle );

    std::string hash = sha256::encode_span( span );
    u8x32 data;
    memcpy( &data, hash.data(), 32 );
    auto canonical = Handle<T>( data, span.size() );

    DatumOrName replacement = canonical;
    std::swap( replacement, local_storage_.at( handle.local_name() ) );
    canonical_storage_.insert_or_assign( canonical, std::move( std::get<OwnedSpan>( replacement ) ) );
    return canonical;

  } else if constexpr ( std::same_as<T, Literal> ) {
    // Literal Blobs: always canonical
    return handle;

  } else if constexpr ( FixTreeType<T> ) {
    // Trees: canonicalize the elements and hash the new contents
    auto old = get_unsync( handle );
    auto tree = OwnedMutTree::allocate( old.size() );
    for ( size_t i = 0; i < old.size(); i++ ) {
      tree[i] = canonicalize( old[i], lock );
    }
    OwnedTree owned( std::move( tree ) );

    std::string hash = sha256::encode_span( owned.span() );
    u8x32 data;
    memcpy( &data, hash.data(), 32 );
    auto canonical = Handle<T>( data, owned.size() );

    // TODO: do we need to maintain a reference count here? another thread might still have a reference to this span
    local_storage_.at( handle.local_name() ) = canonical; // frees the old tree
    canonical_storage_.insert_or_assign( canonical, std::move( owned ) );
    return canonical;
  }
  assert( false );
}

template Handle<Fix> RuntimeStorage::canonicalize( Handle<Fix>, std::unique_lock<std::shared_mutex>& );

void RuntimeStorage::label( Handle<Fix> target, const std::string_view label )
{
  std::unique_lock lock( storage_mutex_ );
  labels_.insert( { target, std::string( label ) } );
}

std::optional<Handle<Fix>> RuntimeStorage::label( const std::string_view label )
{
  std::shared_lock lock( storage_mutex_ );
  for ( const auto& [handle, current] : labels_ ) {
    if ( current == label ) {
      return handle;
    }
  }
  return {};
}

std::unordered_set<std::string> RuntimeStorage::labels( std::optional<Handle<Fix>> handle )
{
  std::shared_lock lock( storage_mutex_ );
  std::unordered_set<std::string> result;
  for ( const auto& [h, l] : labels_ ) {
    if ( !handle or h == handle ) {
      result.insert( l );
    }
  }
  return result;
}

std::filesystem::path RuntimeStorage::get_fix_repo()
{
  auto current_directory = fs::current_path();
  while ( not fs::exists( current_directory / ".fix" ) ) {
    if ( not current_directory.has_parent_path() or current_directory == "/" ) {
      cerr << "Error: not a Fix repository (or any of the parent directories)\n";
      exit( EXIT_FAILURE );
    }
    current_directory = current_directory.parent_path();
  }
  return current_directory / ".fix";
}

Handle<Fix> RuntimeStorage::serialize( Handle<Fix> root, bool pins )
{
  auto canonical = canonicalize( root );
  visit_full(
    canonical,
    [&]( Handle<Fix> current ) {
      std::visit( overload {
                    []( const Handle<Literal> ) {},
                    [&]( const auto x ) {
                      string file_name = base16::encode( current.content );
                      auto path = get_fix_repo() / "data" / file_name;
                      if ( fs::exists( path ) )
                        return;
                      VLOG( 1 ) << "Serializing " << x;
                      std::visit( [&]( auto& arg ) { arg.to_file( path ); },
                                  std::get<OwnedSpan>( canonical_storage_.at( x ) ) );
                    },
                  },
                  fix_data( current ) );
    },
    [&]( Handle<Fix> src, const std::unordered_set<Handle<Fix>>& dsts ) {
      if ( dsts.empty() )
        return;
      string dir_name = base16::encode( src.content );
      fs::create_directory( get_fix_repo() / "pins" / dir_name );
      for ( const auto& dst : dsts ) {
        string file_name = base16::encode( src.content );
        fs::create_symlink( get_fix_repo() / "pins" / dir_name / file_name, "../data/" + dir_name );
        serialize( dst, pins );
      }
    } );
  return canonical;
}

Handle<Fix> RuntimeStorage::serialize( const std::string_view label, bool pins )
{
  for ( auto it = labels_.begin(); it != labels_.end(); ++it ) {
    auto [h, l] = *it;
    if ( l == label ) {
      auto canonical = serialize( h, pins );
      std::string target = base16::encode( canonical.content );
      auto path = get_fix_repo() / "labels" / label;
      if ( fs::exists( path ) ) {
        fs::remove( path );
      }
      fs::create_symlink( "../data/" + target, path );
      return canonical;
    }
  }
  throw std::runtime_error( "invalid label: " + string( label ) );
}

Handle<Fix> read_handle( fs::path file )
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
  auto h = Handle<Fix>::forge( base16::decode( { buf, 64 } ) );
  VLOG( 1 ) << "decoded " << string_view( buf, 64 ) << " into " << h;
  return h;
}

void RuntimeStorage::deserialize()
{
  const auto fix = get_fix_repo();

  for ( const auto& label : fs::directory_iterator( fix / "labels" ) ) {
    auto handle = Handle<Fix>::forge( base16::decode( fs::read_symlink( label ).filename().string() ) );
    labels_.insert( make_pair( handle, label.path().filename() ) );
  }

  for ( const auto& datum : fs::directory_iterator( fix / "data" ) ) {
    VLOG( 2 ) << "deserializing " << datum;
    auto handle = Handle<Fix>::forge( base16::decode( datum.path().filename().string() ) );
    std::visit( overload {
                  []( Handle<Literal> ) {},
                  [&]( Handle<Named> ) { create( OwnedBlob::from_file( datum ), handle ); },
                  [&]( auto x ) {
                    create_tree<Tree<typename decltype( x )::element_type>>( OwnedTree::from_file( datum ),
                                                                             handle );
                  },
                },
                fix_data( handle ) );
  }
}

#if 0
void RuntimeStorage::deserialize_objects( const fs::path& dir )
{
  for ( const auto& file : fs::directory_iterator( dir ) ) {
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
#endif

bool RuntimeStorage::contains( Handle<Fix> handle )
{
  if ( handle.contains<Expression, Value, Thunk>() or handle.contains<Expression, Value, Object, BlobStub>()
       or handle.contains<Expression, Value, Object, ObjectTreeStub>()
       or handle.contains<Expression, Value, ValueTreeStub>()
       or handle.contains<Expression, Value, Object, Blob, Literal>() ) {
    // Thunks, Stubs, and Literal Blobs are self-contained
    return true;
  }

  shared_lock lock( storage_mutex_ );

  if ( fix_is_local( handle ) ) {
    return local_storage_.size() > std::visit( []( const auto x ) { return x.local_name(); }, fix_data( handle ) );
  } else {
    Handle<Fix> raw = std::visit( overload {
                                    []( const Handle<Literal>& literal ) -> Handle<Fix> { return literal; },
                                    []( const Handle<Named>& named ) -> Handle<Fix> { return named; },
                                    []( const auto& tree ) -> Handle<Fix> { return tree.untag(); },
                                  },
                                  fix_data( handle ) );
    return canonical_storage_.contains( raw );
  }
}

vector<string> RuntimeStorage::get_friendly_names( Handle<Fix> handle )
{
  vector<string> names;
  const auto [begin, end] = labels_.equal_range( handle );
  for ( auto it = begin; it != end; it++ ) {
    auto x = *it;
    names.push_back( x.second );
  }
  return names;
}

string RuntimeStorage::get_encoded_name( Handle<Fix> handle )
{
  return base16::encode( handle.content );
}

string RuntimeStorage::get_short_name( Handle<Fix> handle )
{
  handle = canonicalize( handle );
  auto encoded = get_encoded_name( handle );
  return encoded.substr( 0, 7 );
}

std::optional<Handle<Fix>> RuntimeStorage::get_handle( const std::string_view short_name )
{
  std::optional<Handle<Fix>> candidate;
  for ( const auto& [handle, data] : canonical_storage_ ) {
    (void)data;
    std::string name = base16::encode( handle.content );
    if ( name.rfind( short_name, 0 ) == 0 ) {
      if ( candidate )
        return {};
      candidate = handle;
    }
  }
  return candidate;
}

string RuntimeStorage::get_display_name( Handle<Fix> handle )
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

std::optional<Handle<Fix>> RuntimeStorage::lookup( const std::string_view ref )
{
  std::shared_lock lock( storage_mutex_ );
  if ( ref.size() == 64 ) {
    auto handle = Handle<Fix>::forge( base16::decode( ref ) );
    if ( contains( handle ) )
      return handle;
  }
  return label( ref ).or_else( [&] { return get_handle( ref ); } ).or_else( [&] -> std::optional<Handle<Fix>> {
    if ( fs::exists( ref ) ) {
      lock.unlock();
      try {
        return create( OwnedBlob::from_file( ref ) );
      } catch ( fs::filesystem_error ) {
        return {};
      }
    } else {
      return {};
    }
  } );
}

bool RuntimeStorage::compare_handles( Handle<Fix> x, Handle<Fix> y )
{
  if ( x == y )
    return true;
  return canonicalize( x ) == canonicalize( y );
}

bool RuntimeStorage::complete( Handle<Fix> handle )
{
  bool res {};
  visit( handle, [&]( Handle<Fix> h ) { res |= contains( h ); } );
  return res;
}

std::vector<Handle<Fix>> RuntimeStorage::minrepo( Handle<Fix> handle )
{
  vector<Handle<Fix>> handles {};
  visit( handle, [&]( Handle<Fix> h ) { handles.push_back( h ); } );
  return handles;
}

const Program& RuntimeStorage::link( Handle<Blob> handle )
{
  unique_lock lock( storage_mutex_ );
  if ( linked_programs_.contains( handle ) ) {
    return linked_programs_.at( handle );
  }
  Program program = link_program( get( handle ) );
  linked_programs_.insert( { handle, std::move( program ) } );
  return linked_programs_.at( handle );
}

unordered_set<Handle<Fix>> RuntimeStorage::pins( Handle<Fix> handle )
{
  shared_lock lock( storage_mutex_ );
  return pins_[handle];
}

void RuntimeStorage::pin( Handle<Fix> src, Handle<Fix> dst )
{
  unique_lock lock( storage_mutex_ );
  pins_[src].insert( dst );
}

unordered_set<Handle<Fix>> RuntimeStorage::tags( Handle<Fix> handle )
{
  std::shared_lock lock( storage_mutex_ );
  handle = canonicalize( handle );
  unordered_set<Handle<Fix>> result;
  for ( const auto& [name, data] : canonical_storage_ ) {
    std::visit( overload {
                  []( Handle<Named> ) {},
                  []( Handle<Literal> ) {},
                  [&]( auto x ) {
                    if ( x.is_tag() ) {
                      auto tree = x.untag();
                      if ( compare_handles( get( tree )[1], handle ) ) {
                        result.insert( name );
                      }
                    }
                  },
                },
                fix_data( handle ) );
  }

  return result;
}
