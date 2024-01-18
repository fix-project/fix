#include <cassert>
#include <iostream>
#include <stdexcept>

#include <glog/logging.h>
#include <variant>

#include "handle.hh"
#include "handle_util.hh"
#include "object.hh"
#include "overload.hh"
#include "runtimestorage.hh"
#include "storage_exception.hh"

using namespace std;

Handle<Blob> RuntimeStorage::create( BlobData blob, std::optional<Handle<Blob>> name )
{
  auto data = data_.write();
  if ( name ) {
    auto handle = handle::extract<Blob>( *name ).value();
    std::visit(
      [&]( auto x ) {
        assert( x.size() == blob->size() );
        (void)x;
      },
      handle.get() );
    assert( not( handle::is_local( handle ) ) );
    assert( not handle.contains<Literal>() );
    data->canonical_storage_.insert( { handle, blob } );
    return handle;
  } else {
    if ( blob->size() < Handle<Literal>::MAXIMUM_LENGTH ) {
      auto handle = Handle<Literal>( { blob->data(), blob->size() } ).into<Blob>();
      return handle;
    } else {
      uint64_t local_name = data->local_storage_.size();
      auto handle = Handle<Named>( local_name, blob->size() ).into<Blob>();
      data->local_storage_.push_back( blob );
      return handle;
    }
  }
}

Handle<AnyTree> RuntimeStorage::create( TreeData tree, std::optional<Handle<AnyTree>> name )
{
  VLOG( 2 ) << "add " << tree->data() << " " << tree->size();
  FixKind kind = handle::tree_kind( tree );
  auto data = data_.write();
  if ( name ) {
    assert( handle::size( *name ) == tree->size() );
    assert( handle::kind( *name ) == kind );
    auto fix = handle::fix( *name );
    data->canonical_storage_.insert( { fix, tree } );
    return *name;
  } else {
    uint64_t local_name = data->local_storage_.size();
    Handle<AnyTree> handle;
    switch ( kind ) {
      case FixKind::Value:
        VLOG( 2 ) << "creating ValueTree";
        handle = Handle<ValueTree>( local_name, tree->size() );
        break;
      case FixKind::Object:
        VLOG( 2 ) << "creating ObjectTree";
        handle = Handle<ObjectTree>( local_name, tree->size() );
        break;
      case FixKind::Expression:
        VLOG( 2 ) << "creating ExpressionTree";
        handle = Handle<ExpressionTree>( local_name, tree->size() );
        break;
      case FixKind::Fix:
        assert( false );
    }
    data->local_storage_.push_back( tree );
    VLOG( 2 ) << "add " << handle;
    return handle;
  }
}

void RuntimeStorage::create( Handle<Relation> relation, Handle<Object> result )
{
  if ( handle::is_local( relation ) ) {
    data_.write()->local_relations_.insert( { relation, result } );
  } else if ( not handle::is_local( result ) ) {
    // TODO: deal with this more elegantly (maybe have a separate lookup table for canonical relations to local
    // expressions?
    data_.write()->canonical_storage_.insert( { relation, canonicalize( result ) } );
  } else {
    data_.write()->canonical_storage_.insert( { relation, result } );
  }
}

template<FixTreeType T>
Handle<T> RuntimeStorage::create_tree( TreeData tree, std::optional<Handle<AnyTree>> name )
{
  auto anytree = create( tree, name );
  return anytree.visit<Handle<T>>( overload {
    [&]( auto t ) -> Handle<T> {
      if constexpr ( std::constructible_from<Handle<T>, decltype( t )> ) {
        return Handle<T>( t );
      } else {
        throw std::runtime_error( "tried to create tree with incorrect type" );
      }
    },
  } );
}

template Handle<Tree<Value>> RuntimeStorage::create_tree( TreeData, std::optional<Handle<AnyTree>> );
template Handle<Tree<Object>> RuntimeStorage::create_tree( TreeData, std::optional<Handle<AnyTree>> );
template Handle<Tree<Expression>> RuntimeStorage::create_tree( TreeData, std::optional<Handle<AnyTree>> );

BlobData RuntimeStorage::get( Handle<Named> handle )
{
  auto data = data_.read();
  if ( handle.is_local() ) {
    const uint64_t local_name = handle.local_name();
    VLOG( 2 ) << "get " << handle << ": local @ " << local_name;
    auto& entry = data->local_storage_.at( local_name );
    if ( std::holds_alternative<Handle<Fix>>( entry ) ) {
      const auto other = std::get<Handle<Fix>>( entry );
      VLOG( 2 ) << "get " << handle << " -> canonical @ " << other;
      VLOG( 2 ) << data->canonical_storage_.at( other ).index();
      return std::get<BlobData>( data->canonical_storage_.at( other ) );
    }
    return std::get<BlobData>( entry );
  } else {
    VLOG( 2 ) << "get " << handle << ": canonical";
    return std::get<BlobData>( data->canonical_storage_.at( handle ) );
  }
}

TreeData RuntimeStorage::get( Handle<AnyTree> handle )
{
  VLOG( 2 ) << "get " << handle;
  using MaybeHandle = std::optional<Handle<AnyTree>>;
  auto next = overload {
    [&]( Handle<ExpressionTree> x ) -> MaybeHandle { return Handle<ObjectTree>( x.hash(), x.size() ); },
    [&]( Handle<ObjectTree> x ) -> MaybeHandle { return Handle<ValueTree>( x.hash(), x.size() ); },
    [&]( Handle<ValueTree> ) -> MaybeHandle { return {}; },
  };
  auto data = data_.read();
  if ( handle::is_local( handle ) ) {
    const uint64_t local_name = handle::local_name( handle );
    VLOG( 2 ) << "get " << handle << ": local @ " << local_name;
    auto& entry = data->local_storage_.at( local_name );
    if ( std::holds_alternative<Handle<Fix>>( entry ) ) {
      const auto other = std::get<Handle<Fix>>( entry );
      VLOG( 2 ) << "get " << handle << " -> canonical @ " << other;
      return std::get<TreeData>( data->canonical_storage_.at( other ) );
    }
    return std::get<TreeData>( entry );
  } else {
    MaybeHandle current = handle;
    while ( current ) {
      Handle<AnyTree> x = *current;
      auto fix = handle::fix( x );
      if ( not data->canonical_storage_.contains( fix ) ) {
        current = x.visit<MaybeHandle>( next );
        if ( current ) {
          VLOG( 2 ) << x << " not found, going to " << *current;
        } else {
          VLOG( 2 ) << x << " not found";
        }
        continue;
      }
      VLOG( 2 ) << "get " << handle << ": canonical";
      return std::get<TreeData>( data->canonical_storage_.at( fix ) );
    }
    throw HandleNotFound( handle::fix( handle ) );
  }
}

Handle<Object> RuntimeStorage::get( Handle<Relation> handle )
{
  auto data = data_.read();
  if ( handle::is_local( handle ) ) {
    VLOG( 2 ) << "get " << handle << ": local";
    return data->local_relations_.at( handle );
  } else {
    VLOG( 2 ) << "get " << handle << ": canonical";
    return std::get<Handle<Object>>( data->canonical_storage_.at( handle ) );
  }
}

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
    if ( not( std::same_as<T, Thunk> or std::same_as<T, ValueTreeRef> or std::same_as<T, ValueTreeRef> ) ) {
      std::visit( [&]( const auto x ) { visit( x, visitor, visited ); }, handle.get() );
    }
    if constexpr ( std::same_as<T, Relation> ) {
      auto target = get( handle );
      std::visit( [&]( const auto x ) { visit( x, visitor, visited ); }, target.get() );
    }
  } else {
    if constexpr ( FixTreeType<T> ) {
      auto tree = get( handle );
      for ( const auto& element : tree->span() ) {
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
      for ( const auto& element : tree->span() ) {
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
  if ( not handle::is_local( handle ) ) {
    return handle;
  }

  if constexpr ( not Handle<T>::is_fix_sum_type ) {
    VLOG( 2 ) << "canonicalizing " << handle;
  }

  if constexpr ( Handle<T>::is_fix_sum_type ) {
    // non-terminal types: recurse until terminal
    auto lambda = [&]( const auto x ) -> Handle<T> { return Handle<T>( canonicalize( x ) ); };
    Handle<T> canonical = std::visit( lambda, handle.get() );
    if constexpr ( std::same_as<Handle<Apply>, Handle<T>> or std::same_as<Handle<Eval>, Handle<T>> ) {
      // relations are special; we canonicalize both sides and rewrite the local relation
      auto target = get( Handle<Relation>( handle ) );
      auto canonical_target = canonicalize( target );

      auto data = data_.write();
      data->local_relations_[handle] = canonical_target;
      data->canonical_storage_.insert( { canonical, canonical_target } );
    }
    return canonical;

  } else if constexpr ( std::same_as<T, Named> ) {
    auto entry = data_.read()->local_storage_.at( handle.local_name() );
    if ( std::holds_alternative<Handle<Fix>>( entry ) ) {
      return std::get<Handle<Fix>>( entry )
        .template try_into<Expression>()
        .and_then( []( auto x ) { return x.template try_into<Object>(); } )
        .and_then( []( auto x ) { return x.template try_into<Value>(); } )
        .and_then( []( auto x ) { return x.template try_into<Blob>(); } )
        .and_then( []( auto x ) { return x.template try_into<Named>(); } )
        .value();
    }
    // Named Blobs: hash the contents
    auto blob = get( handle );

    auto canonical = handle::create( blob ).template unwrap<Named>();

    Data::LocalEntry replacement = canonical;
    auto data = data_.write();
    std::swap( replacement, data->local_storage_.at( handle.local_name() ) );
    data->canonical_storage_.insert_or_assign( canonical, std::get<BlobData>( replacement ) );
    return canonical;

  } else if constexpr ( std::same_as<T, Literal> ) {
    // Literal Blobs: always canonical
    return handle;

  } else if constexpr ( FixTreeType<T> ) {
    // Trees: canonicalize the elements and hash the new contents
    auto old = get( handle );
    auto tree = OwnedMutTree::allocate( old->size() );
    for ( size_t i = 0; i < old->size(); i++ ) {
      tree[i] = canonicalize( old->at( i ) );
    }

    auto created = std::make_shared<OwnedTree>( std::move( tree ) );
    auto canonical = handle::tree_unwrap<T>( handle::create( created ) );

    auto data = data_.write();
    data->local_storage_[handle.local_name()] = canonical; // frees the old tree
    data->canonical_storage_.insert_or_assign( canonical, created );
    return canonical;
  }
  assert( false );
}

template Handle<Fix> RuntimeStorage::canonicalize( Handle<Fix> );

void RuntimeStorage::label( Handle<Fix> target, const std::string_view label )
{
  data_.write()->labels_.insert( { std::string( label ), target } );
}

Handle<Fix> RuntimeStorage::labeled( const std::string_view label )
{
  std::string l( label );
  auto data = data_.read();
  if ( data->labels_.contains( l ) ) {
    return data->labels_.at( l );
  } else {
    throw LabelNotFound( label );
  }
}

std::unordered_set<std::string> RuntimeStorage::labels()
{
  auto data = data_.read();
  std::unordered_set<std::string> result;
  for ( const auto& [l, h] : data->labels_ ) {
    result.insert( l );
  }
  return result;
}

/* Handle<Fix> RuntimeStorage::serialize( Handle<Fix> root, bool pins ) */
/* { */
/*   auto canonical = canonicalize( root ); */
/*   visit_full( */
/*     canonical, */
/*     [&]( Handle<Fix> current ) { */
/*       std::visit( overload { */
/*                     []( const Handle<Literal> ) {}, */
/*                     [&]( const Handle<Named> name ) { */
/*                       if ( repo_.contains( name ) ) */
/*                         return; */
/*                       auto& span = std::get<Data>( canonical_storage_.at( name ) ); */
/*                       auto& owned = std::get<BlobData>( span ); */
/*                       repo_.put( name, owned ); */
/*                     }, */
/*                     [&]( const auto name ) { */
/*                       if ( repo_.contains( name ) ) */
/*                         return; */
/*                       auto& span = std::get<Data>( canonical_storage_.at( name ) ); */
/*                       auto& owned = std::get<TreeData>( span ); */
/*                       repo_.put( name, owned ); */
/*                     }, */
/*                   }, */
/*                   handle::data( current ).get() ); */
/*     }, */
/*     [&]( Handle<Fix> src, const std::unordered_set<Handle<Fix>>& dsts ) { */
/*       if ( pins ) */
/*         repo_.pin( src, dsts ); */
/*     } ); */
/*   return canonical; */
/* } */

/* Handle<Fix> RuntimeStorage::serialize( const std::string_view label, bool pins ) */
/* { */
/*   auto handle = labels_.at( std::string( label ) ); */
/*   auto canonical = serialize( handle, pins ); */
/*   repo_.label( label, canonical ); */
/*   return canonical; */
/* } */

bool RuntimeStorage::contains( Handle<Fix> handle )
{
  if ( handle.contains<Expression, Object, Thunk>() or handle.contains<Expression, Object, Value, BlobRef>()
       or handle.contains<Expression, Object, Value, ValueTreeRef>()
       or handle.contains<Expression, Object, ValueTreeRef>()
       or handle.contains<Expression, Object, Value, Blob, Literal>() ) {
    // Thunks, Refs, and Literal Blobs are self-contained
    return true;
  }

  auto data = data_.read();
  if ( handle::is_local( handle ) ) {
    if ( handle.contains<Relation>() ) {
      if ( data->local_relations_.contains( handle.unwrap<Relation>() ) )
        return true;
      auto local_name = handle::local_name( handle );
      auto entry = data->local_storage_.at( local_name );
      if ( std::holds_alternative<Handle<Fix>>( entry ) ) {
        auto canon = std::get<Handle<Fix>>( entry );
        auto canon_relation = handle.unwrap<Relation>().visit<Handle<Fix>>( overload {
          [&]( Handle<Apply> ) {
            return Handle<Apply>( handle::tree_unwrap<ObjectTree>( canon.unwrap<Expression>() ) );
          },
          [&]( Handle<Eval> ) { return Handle<Eval>( canon.unwrap<Expression>().unwrap<Object>() ); },
        } );
        if ( data->canonical_storage_.contains( canon_relation ) ) {
          return true;
        }
        return false;
      }
    }
    return data->local_storage_.size()
           > std::visit( []( const auto x ) { return x.local_name(); }, handle::data( handle ).get() );
  } else {
    if ( handle.contains<Relation>() ) {
      return data->canonical_storage_.contains( handle );
    } else {
      Handle<Fix> raw = std::visit( overload {
                                      []( const Handle<Literal>& literal ) -> Handle<Fix> { return literal; },
                                      []( const Handle<Named>& named ) -> Handle<Fix> { return named; },
                                      []( const auto& tree ) -> Handle<Fix> { return tree.untag(); },
                                    },
                                    handle::data( handle ).get() );
      if ( data->canonical_storage_.contains( raw ) ) {
        return true;
      }
      return false;
    }
  }
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
  auto data = data_.read();
  if ( data->pins_.contains( handle ) ) {
    return data->pins_.at( handle );
  } else {
    return {};
  }
}

void RuntimeStorage::pin( Handle<Fix> src, Handle<Fix> dst )
{
  auto data = data_.write();
  data->pins_.at( src ).insert( dst );
}

unordered_set<Handle<Fix>> RuntimeStorage::tags( Handle<Fix> handle )
{
  auto data = data_.read();
  handle = canonicalize( handle );
  unordered_set<Handle<Fix>> result;
  for ( const auto& [name, data] : data->canonical_storage_ ) {
    std::visit( overload {
                  []( Handle<Named> ) {},
                  []( Handle<Literal> ) {},
                  [&]( auto x ) {
                    if ( x.is_tag() ) {
                      auto tree = x.untag();
                      if ( compare_handles( get( tree )->at( 1 ), handle ) ) {
                        result.insert( name );
                      }
                    }
                  },
                },
                handle::data( handle ).get() );
  }

  return result;
}
