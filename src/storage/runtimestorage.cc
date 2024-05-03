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

bool RuntimeStorage::complete( Handle<Fix> handle )
{
  bool res {};
  visit( handle, [&]( Handle<Fix> h ) {
    res |= handle::data( h ).visit<bool>(
      overload { []( Handle<Literal> ) { return true; }, [&]( auto h ) { return contains( h ); } } );
  } );
  return res;
}

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
