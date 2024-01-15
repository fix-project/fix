#include <fstream>
#include <iostream>
#include <shared_mutex>
#include <stdexcept>

#include <glog/logging.h>
#include <variant>

#include "base16.hh"
#include "handle.hh"
#include "object.hh"
#include "overload.hh"
#include "runtimestorage.hh"
#include "sha256.hh"
#include "storage_exception.hh"

using namespace std;
namespace fs = std::filesystem;

Handle<Blob> RuntimeStorage::create( OwnedBlob&& blob, std::optional<Handle<Fix>> name )
{
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
  if ( fix_is_local( relation ) ) {
    local_relations_.insert( { relation, result } );
  } else if ( not fix_is_local( result ) ) {
    LOG( WARNING ) << "attempted to relate canonical handle to local handle";
    canonical_storage_.insert( { relation, canonicalize( result ) } );
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

BlobSpan RuntimeStorage::get( const Handle<Blob>& handle )
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
    if ( canonical_storage_.contains( handle ) ) {
      return std::get<OwnedBlob>( std::get<OwnedSpan>( canonical_storage_.at( handle ) ) ).span();
    } else {
      auto blob = repo_.get( handle.unwrap<Named>() );
      auto span = blob.span();
      canonical_storage_.insert( { handle, std::move( blob ) } );
      return span;
    }
  }
}

template<FixTreeType T>
TreeSpan RuntimeStorage::get( Handle<T> handle )
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
    if ( canonical_storage_.contains( handle ) ) {
      return std::get<OwnedTree>( std::get<OwnedSpan>( canonical_storage_.at( handle ) ) ).span();
    } else {
      auto tree = repo_.get( handle );
      auto span = tree.span();
      canonical_storage_.insert( { handle, std::move( tree ) } );
      return span;
    }
  }
}

Handle<Fix> RuntimeStorage::get( Handle<Relation> handle )
{
  if ( fix_is_local( handle ) ) {
    VLOG( 2 ) << "get " << handle << ": local";
    return local_relations_.at( handle );
  } else {
    VLOG( 2 ) << "get " << handle << ": canonical";
    if ( canonical_storage_.contains( handle ) ) {
      return std::get<Handle<Fix>>( canonical_storage_.at( handle ) );
    } else {
      auto result = repo_.get( handle );
      canonical_storage_.insert( { handle, result } );
      return result;
    }
  }
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
    std::unordered_set<Handle<Fix>> pins = pinned( handle );
    for ( const auto& dst : pins ) {
      visit_full( dst, visitor, pin_visitor, visited );
      visitor( dst );
    }
    ( *pin_visitor )( handle, pins );
  }
}

template void RuntimeStorage::visit_full(
  Handle<Fix>,
  std::function<void( Handle<Fix> )>,
  std::optional<std::function<void( Handle<Fix>, const std::unordered_set<Handle<Fix>>& )>>,
  std::unordered_set<Handle<Fix>> );

template<FixType T>
Handle<T> RuntimeStorage::canonicalize( Handle<T> handle )
{
  if ( not fix_is_local( handle ) ) {
    return handle;
  }

  if constexpr ( not Handle<T>::is_fix_sum_type ) {
    VLOG( 2 ) << "canonicalizing " << handle;
  }

  if constexpr ( Handle<T>::is_fix_sum_type ) {
    // non-terminal types: recurse until terminal
    auto canonical = std::visit( [&]( const auto x ) { return Handle<T>( canonicalize( x ) ); }, handle.get() );
    if constexpr ( std::constructible_from<Handle<Relation>, Handle<T>> ) {
      // relations are special; we canonicalize both sides and rewrite the local relation
      auto target = get( handle );
      auto canonical_target = canonicalize( target );

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
    auto span = get( handle );

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
    auto old = get( handle );
    auto tree = OwnedMutTree::allocate( old.size() );
    for ( size_t i = 0; i < old.size(); i++ ) {
      tree[i] = canonicalize( old[i] );
    }
    OwnedTree owned( std::move( tree ) );

    std::string hash = sha256::encode_span( owned.span() );
    u8x32 data;
    memcpy( &data, hash.data(), 32 );
    auto canonical = Handle<T>( data, owned.size() );

    local_storage_.at( handle.local_name() ) = canonical; // frees the old tree
    canonical_storage_.insert_or_assign( canonical, std::move( owned ) );
    return canonical;
  }
  assert( false );
}

template Handle<Fix> RuntimeStorage::canonicalize( Handle<Fix> );

void RuntimeStorage::label( Handle<Fix> target, const std::string_view label )
{
  labels_.insert( { std::string( label ), target } );
}

Handle<Fix> RuntimeStorage::labeled( const std::string_view label )
{
  std::string l( label );
  if ( labels_.contains( l ) ) {
    return labels_.at( l );
  } else {
    auto result = repo_.labeled( l );
    labels_.insert( { l, result } );
    return result;
  }
}

std::unordered_set<std::string> RuntimeStorage::labels() const
{
  std::unordered_set<std::string> result;
  for ( const auto& [l, h] : labels_ ) {
    result.insert( l );
  }
  for ( const auto& l : repo_.labels() ) {
    result.insert( l );
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
                    [&]( const Handle<Named> name ) {
                      if ( repo_.contains( name ) )
                        return;
                      auto& span = std::get<OwnedSpan>( canonical_storage_.at( name ) );
                      auto& owned = std::get<OwnedBlob>( span );
                      repo_.put( name, owned );
                    },
                    [&]( const auto name ) {
                      if ( repo_.contains( name ) )
                        return;
                      auto& span = std::get<OwnedSpan>( canonical_storage_.at( name ) );
                      auto& owned = std::get<OwnedTree>( span );
                      repo_.put( name, owned );
                    },
                  },
                  fix_data( current ) );
    },
    [&]( Handle<Fix> src, const std::unordered_set<Handle<Fix>>& dsts ) {
      if ( pins )
        repo_.pin( src, dsts );
    } );
  return canonical;
}

Handle<Fix> RuntimeStorage::serialize( const std::string_view label, bool pins )
{
  auto handle = labels_.at( std::string( label ) );
  auto canonical = serialize( handle, pins );
  repo_.label( label, canonical );
  return canonical;
}

bool RuntimeStorage::contains( Handle<Fix> handle )
{
  if ( handle.contains<Expression, Value, Thunk>() or handle.contains<Expression, Value, Object, BlobStub>()
       or handle.contains<Expression, Value, Object, ObjectTreeStub>()
       or handle.contains<Expression, Value, ValueTreeStub>()
       or handle.contains<Expression, Value, Object, Blob, Literal>() ) {
    // Thunks, Stubs, and Literal Blobs are self-contained
    return true;
  }

  if ( fix_is_local( handle ) ) {
    return local_storage_.size() > std::visit( []( const auto x ) { return x.local_name(); }, fix_data( handle ) );
  } else {
    Handle<Fix> raw = std::visit( overload {
                                    []( const Handle<Literal>& literal ) -> Handle<Fix> { return literal; },
                                    []( const Handle<Named>& named ) -> Handle<Fix> { return named; },
                                    []( const auto& tree ) -> Handle<Fix> { return tree.untag(); },
                                  },
                                  fix_data( handle ) );
    if ( canonical_storage_.contains( raw ) ) {
      return true;
    }
    return repo_.contains( raw );
  }
}

vector<string> RuntimeStorage::get_friendly_names( Handle<Fix> handle )
{
  vector<string> names;
  for ( const auto& [l, h] : labels_ ) {
    if ( h == handle ) {
      names.push_back( l );
    }
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

Handle<Fix> RuntimeStorage::get_handle( const std::string_view short_name )
{
  std::optional<Handle<Fix>> candidate;
  for ( const auto& [handle, data] : canonical_storage_ ) {
    (void)data;
    std::string name = base16::encode( handle.content );
    if ( name.rfind( short_name, 0 ) == 0 ) {
      if ( candidate )
        throw AmbiguousReference( short_name );
      candidate = handle;
    }
  }
  for ( const auto handle : repo_.data() ) {
    std::string name = base16::encode( handle.content );
    if ( name.rfind( short_name, 0 ) == 0 ) {
      if ( candidate )
        throw AmbiguousReference( short_name );
      candidate = handle;
    }
  }
  if ( candidate )
    return *candidate;
  throw ReferenceNotFound( short_name );
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

Handle<Fix> RuntimeStorage::lookup( const std::string_view ref )
{
  if ( ref.size() == 64 ) {
    auto handle = Handle<Fix>::forge( base16::decode( ref ) );
    if ( contains( handle ) )
      return handle;
    if ( repo_.contains( handle ) )
      return handle;
  }
  try {
    return labeled( ref );
  } catch ( LabelNotFound ) {}
  try {
    return get_handle( ref );
  } catch ( ReferenceNotFound ) {}
  if ( fs::exists( ref ) ) {
    try {
      return create( OwnedBlob::from_file( ref ) );
    } catch ( fs::filesystem_error ) {}
  }
  throw ReferenceNotFound( ref );
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

unordered_set<Handle<Fix>> RuntimeStorage::pinned( Handle<Fix> handle )
{
  if ( pins_.contains( handle ) ) {
    return pins_[handle];
  } else {
    pins_.insert( { handle, repo_.pinned( handle ) } );
    return pins_[handle];
  }
}

void RuntimeStorage::pin( Handle<Fix> src, Handle<Fix> dst )
{
  pins_[src].insert( dst );
}

unordered_set<Handle<Fix>> RuntimeStorage::tags( Handle<Fix> handle )
{
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
