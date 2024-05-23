#include <stdexcept>

#include <glog/logging.h>
#include <string_view>
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
  auto handle = name.or_else( [&] -> decltype( name ) { return handle::create( blob ); } ).value();
  handle.visit<void>( overload {
    [&]( Handle<Literal> ) {},
    [&]( Handle<Named> name ) {
      blobs_.write()->insert( { name, blob } );
    },
  } );
  return handle;
}

Handle<AnyTree> RuntimeStorage::create( TreeData tree, std::optional<Handle<AnyTree>> name )
{
  auto handle = name.or_else( [&] -> decltype( name ) { return handle::create( tree ); } ).value();
  handle.visit<void>( overload {
    [&]( auto name ) {
      trees_.write()->insert( { handle::upcast( name ).untag(), tree } );
    },
  } );
  return handle;
}

void RuntimeStorage::create( Handle<Object> result, Handle<Relation> relation )
{
  relations_.write()->insert( { relation, result } );
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
  VLOG( 3 ) << "get " << handle;
  auto blobs = blobs_.read();
  if ( blobs->contains( handle ) ) {
    return blobs->at( handle );
  }
  throw HandleNotFound( handle );
}

TreeData RuntimeStorage::get( Handle<AnyTree> handle )
{
  VLOG( 3 ) << "get " << handle;
  auto trees = trees_.read();
  auto generic = handle::upcast( handle ).untag();
  if ( trees->contains( generic ) ) {
    return trees->at( generic );
  }
  throw HandleNotFound( handle::fix( handle ) );
}

Handle<Object> RuntimeStorage::get( Handle<Relation> handle )
{
  auto rels = relations_.read();
  if ( rels->contains( handle ) ) {
    return rels->at( handle );
  }
  throw HandleNotFound( handle );
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
    if ( not( std::same_as<T, Thunk> or std::same_as<T, BlobRef> ) ) {
      std::visit( [&]( const auto x ) { visit( x, visitor, visited ); }, handle.get() );
    }

    if constexpr ( std::same_as<T, Relation> ) {
      auto target = get( handle );
      std::visit( [&]( const auto x ) { visit( x, visitor, visited ); }, target.get() );
    }
  } else if constexpr ( std::same_as<T, ValueTreeRef> or std::same_as<T, ObjectTreeRef> ) {
    return;
  } else {
    if constexpr ( FixTreeType<T> ) {
      auto tree = get( handle );
      for ( const auto& element : tree->span() ) {
        visit( element, visitor, visited );
      }
    }
    VLOG( 3 ) << "visiting " << handle;
    visitor( handle );
    visited.insert( handle );
  }
}

#if 0
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
    VLOG( 3 ) << "full-visiting " << handle;
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
#endif

void RuntimeStorage::label( Handle<Fix> target, const std::string_view label )
{
  labels_.write()->insert( { std::string( label ), target } );
}

Handle<Fix> RuntimeStorage::labeled( const std::string_view label )
{
  std::string l( label );
  auto labels = labels_.read();
  if ( labels->contains( l ) ) {
    return labels->at( l );
  }
  throw LabelNotFound( label );
}

bool RuntimeStorage::contains( const std::string_view label )
{
  auto labels = labels_.read();
  return labels->contains( label );
}

std::unordered_set<std::string> RuntimeStorage::labels()
{
  auto labels = labels_.read();
  std::unordered_set<std::string> result;
  for ( const auto& [l, h] : labels.get() ) {
    (void)h;
    result.insert( l );
  }
  return result;
}

#if 0
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
                      auto& span = std::get<Data>( canonical_storage_.at( name ) );
                      auto& owned = std::get<BlobData>( span );
                      repo_.put( name, owned );
                    },
                    [&]( const auto name ) {
                      if ( repo_.contains( name ) )
                        return;
                      auto& span = std::get<Data>( canonical_storage_.at( name ) );
                      auto& owned = std::get<TreeData>( span );
                      repo_.put( name, owned );
                    },
                  },
                  handle::data( current ).get() );
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
#endif

bool RuntimeStorage::contains( Handle<Named> handle )
{
  return blobs_.read()->contains( handle );
}

bool RuntimeStorage::contains( Handle<AnyTree> handle )
{
  return trees_.read()->contains( handle::upcast( handle ).untag() );
}

bool RuntimeStorage::contains( Handle<Relation> handle )
{
  return relations_.read()->contains( handle );
}

optional<Handle<AnyTree>> RuntimeStorage::contains( Handle<AnyTreeRef> handle )
{
  auto tmp_tree = handle.visit<Handle<AnyTree>>(
    overload { []( Handle<ValueTreeRef> r ) { return Handle<ValueTree>( r.content, 0 ); },
               []( Handle<ObjectTreeRef> r ) { return Handle<ObjectTree>( r.content, 0 ); } } );

  auto trees = trees_.read();
  auto entry = trees->find( handle::upcast( tmp_tree ) );

  if ( entry == trees->end() ) {
    return {};
  }

  auto et = entry->first;

  // Cast to same kind as Handle<AnyTreeRef>
  auto res_tree = handle.visit<Handle<AnyTree>>( overload {
    [&]( Handle<ValueTreeRef> r ) { return Handle<ValueTree>( et.content, et.size(), r.is_tag() ); },
    [&]( Handle<ObjectTreeRef> r ) { return Handle<ObjectTree>( et.content, et.size(), r.is_tag() ); } } );

  return res_tree;
}

Handle<AnyTreeRef> RuntimeStorage::ref( Handle<AnyTree> handle )
{
  return handle.visit<Handle<AnyTreeRef>>( overload {
    [&]( Handle<ValueTree> t ) { return t.into<ValueTreeRef>( get( handle )->size() ); },
    [&]( Handle<ObjectTree> t ) { return t.into<ObjectTreeRef>( get( handle )->size() ); },
    [&]( Handle<ExpressionTree> ) -> Handle<AnyTreeRef> {
      throw runtime_error( "ExpressionTree cannot be reffed" );
    },
  } );
}

#if 0
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
#endif

#if 0
bool RuntimeStorage::compare_handles( Handle<Fix> x, Handle<Fix> y )
{
  if ( x == y )
    return true;
  return canonicalize( x ) == canonicalize( y );
}
#endif

#if 0
std::vector<Handle<Fix>> RuntimeStorage::minrepo( Handle<Fix> handle )
{
  vector<Handle<Fix>> handles {};
  visit( handle, [&]( Handle<Fix> h ) { handles.push_back( h ); } );
  return handles;
}
#endif

unordered_set<Handle<Fix>> RuntimeStorage::pinned( Handle<Fix> handle )
{
  auto pins = pins_.read();
  if ( pins->contains( handle ) ) {
    return pins->at( handle );
  } else {
    return {};
  }
}

void RuntimeStorage::pin( Handle<Fix> src, Handle<Fix> dst )
{
  pins_.write()->at( src ).insert( dst );
}

#if 0
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
#endif
