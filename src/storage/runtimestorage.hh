#pragma once

#include <absl/container/flat_hash_set.h>
#include <stdio.h>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

#include "handle.hh"
#include "handle_util.hh"
#include "hash_table.hh"
#include "mutex.hh"
#include "object.hh"

#include "absl/container/flat_hash_map.h"

#ifndef TRUSTED
#define TRUSTED
#endif

struct AbslHash
{
  template<typename T>
  std::size_t operator()( Handle<T> const& handle ) const noexcept
  {
    u64x4 dwords = (u64x4)handle.content;
    return absl::Hash<uint64_t> {}( dwords[0] );
  }
};

class RuntimeStorage
{
private:
  friend class RuntimeWorker;
  using BlobMap = FixTable<Named, BlobData, AbslHash>;
  using TreeMap = FixTable<ExpressionTree, TreeData, AbslHash, handle::tree_equal>;
  using RelationMap = FixTable<Fix, Handle<Object>, AbslHash>;

  using PinMap = absl::flat_hash_map<Handle<Fix>, std::unordered_set<Handle<Fix>>, AbslHash>;
  using LabelMap = absl::flat_hash_map<std::string, Handle<Fix>>;

  BlobMap blobs_ { 1000000 };
  TreeMap trees_ { 1000000 };
  RelationMap relations_ { 1000000 };

  SharedMutex<PinMap> pins_ {};
  SharedMutex<LabelMap> labels_ {};

public:
  RuntimeStorage() {}

  // Construct a Blob by taking ownership of a memory region
  Handle<Blob> create( BlobData blob, std::optional<Handle<Blob>> name = {} );

  // Construct a Tree by taking ownership of a memory region
  Handle<AnyTree> create( TreeData tree, std::optional<Handle<AnyTree>> name = {} );

  // Construct a Relation
  void create( Handle<Object> result, Handle<Relation> relation );

  template<FixTreeType T>
  Handle<T> create_tree( TreeData tree, std::optional<Handle<AnyTree>> name = {} );

  // Construct a Blob by copying a memory region.
  Handle<Blob> create( const std::string_view string )
  {
    auto blob = OwnedMutBlob::allocate( string.size() );
    memcpy( blob.data(), string.data(), string.size() );
    return create( std::make_shared<OwnedBlob>( std::move( blob ) ) );
  }

  // Construct a Tree by copying a memory region.
  Handle<AnyTree> create( const std::span<const Handle<Fix>> span )
  {
    auto tree = OwnedMutTree::allocate( span.size() );
    memcpy( tree.data(), span.data(), span.size_bytes() );
    return create( std::make_shared<OwnedTree>( std::move( tree ) ) );
  }

  template<FixTreeType T>
  Handle<T> create_tree( const std::span<const Handle<typename Handle<T>::element_type>> span )
  {
    auto tree = OwnedMutTree::allocate( span.size() );
    for ( size_t i = 0; i < span.size(); i++ ) {
      tree[i] = span[i];
    }
    return create_tree<T>( std::make_shared<OwnedTree>( std::move( tree ) ) );
  }

  // Construct a Tree by copying individual elements.
  template<FixHandle... As>
  Handle<AnyTree> construct( As... args )
  {
    return ( [&]( std::initializer_list<Handle<Fix>> list ) {
      return create( { std::data( list ), list.size() } );
    } )( { args... } );
  }

  template<FixTreeType T, FixHandle... As>
  Handle<T> construct_tree( As... args )
  {
    return ( [&]( std::initializer_list<Handle<typename Handle<T>::element_type>> list ) {
      return create_tree<T>( { std::data( list ), list.size() } );
    } )( { args... } );
  }

  // Return reference to blob content.
  std::shared_ptr<OwnedBlob> get( Handle<Named> name );
  // Return reference to tree content.
  std::shared_ptr<OwnedTree> get( Handle<AnyTree> name );
  // Get the result of a relation.
  Handle<Object> get( Handle<Relation> name );

  Handle<Value> get_relation( Handle<Eval> name ) { return get( name ).unwrap<Value>(); }
  Handle<Object> get_relation( Handle<Apply> name ) { return get( name ); }

  // Convert a Handle into the canonically-named version of that handle.
  template<FixType T>
  Handle<T> canonicalize( Handle<T> handle );

  void label( Handle<Fix> target, const std::string_view label );
  Handle<Fix> labeled( const std::string_view label );
  bool contains( const std::string_view label );
  std::unordered_set<std::string> labels();

  Handle<Fix> serialize( Handle<Fix> root, bool pins = true );
  Handle<Fix> serialize( const std::string_view label, bool pins = true );

  // Tests if the data corresponding to the current handle is stored.
  bool contains( Handle<Named> handle );
  bool contains( Handle<AnyTree> handle );
  bool contains( Handle<Relation> handle );

  // return the reffed Handle<AnyTree> if known
  std::optional<Handle<AnyTree>> contains( Handle<AnyTreeRef> handle );
  Handle<AnyTreeRef> ref( Handle<AnyTree> tree );

  /**
   * Call @p visitor for every Handle in the "minimum repo" of @p root, i.e., the set of Handles which are needed
   * to execute a procedure which receives @p root in its input.
   *
   * The iteration order is such that every child will be visited before its parents.
   *
   * @param root            The Handle from which to start traversing inputs.
   * @param visitor         A function to call on every dependency.
   */
  template<FixType T>
  void visit( Handle<T> root,
              std::function<void( Handle<Fix> )> visitor,
              std::unordered_set<Handle<Fix>> visited = {} );

  /**
   * Call @p visitor for every Handle in the "fully accessible repo" of @p root, i.e., the set of Handles which
   * might ever be needed in the recursive evaluation of a procedure which receives @p root in its input.
   *
   * The iteration order is such that every child will be visited before its parents.
   *
   * @param root            The Handle from which to start traversing inputs.
   * @param visitor         A function to call on every dependency.
   */
  template<FixType T>
  void visit_full(
    Handle<T> root,
    std::function<void( Handle<Fix> )> visitor,
    std::optional<std::function<void( Handle<Fix>, const std::unordered_set<Handle<Fix>>& )>> pin_visitor
    = std::nullopt,
    std::unordered_set<Handle<Fix>> visited = {} );

  /**
   * Determines if two Handles should be treated as equal.  This might canonicalize the Handles if necessary.
   *
   * @return  true if the Handles refer to the same object, false otherwise
   */
  bool compare_handles( Handle<Fix> x, Handle<Fix> y );

  /**
   * Computes the minimum repository of @p root.
   */
  std::vector<Handle<Fix>> minrepo( Handle<Fix> root );

  /**
   * Add an advisory pin to @p dst from @p src.
   */
  void pin( Handle<Fix> src, Handle<Fix> dst );

  /**
   * Return handles pinned by @p handle.
   */
  std::unordered_set<Handle<Fix>> pinned( Handle<Fix> handle );

  /**
   * Return handles of tags that tag @p handle.
   */
  std::unordered_set<Handle<Fix>> tags( Handle<Fix> handle );

  Handle<Object> wait( Handle<Relation> handle )
  {
    using namespace std::chrono_literals;

    while ( !relations_.contains( handle ) ) {
      std::this_thread::sleep_for( 100ms );
    }

    return get( handle );
  }
};
